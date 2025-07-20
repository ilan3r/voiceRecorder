/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum {
	UNKNOWN,
	HALF_COMPLETED,
	FULL_COMPLETED
}  CallBack_Result_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

#define WAV_WRITE_SAMPLE_COUNT 2048

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2S_HandleTypeDef hi2s2;
I2S_HandleTypeDef hi2s3;
DMA_HandleTypeDef hdma_spi2_tx;
DMA_HandleTypeDef hdma_spi3_rx;

SPI_HandleTypeDef hspi1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

FATFS fatfs;
FIL fil;
FRESULT fresult;

int16_t samples[32000];

uint32_t fread_size = 0;
uint32_t recording_size = 0;
uint32_t played_size = 0;

uint8_t playback = 1;



volatile CallBack_Result_t callback_result = UNKNOWN;

volatile uint8_t half_i2s, full_i2s;

volatile uint8_t button_flag, start_stop_recording;
int16_t data_i2s[WAV_WRITE_SAMPLE_COUNT];
volatile int16_t sample_i2s;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_I2S2_Init(void);
static void MX_I2S3_Init(void);
/* USER CODE BEGIN PFP */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef * hi2s);
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef * hi2s);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_13)  // Replace with your pin
    {
        // Your interrupt handling code here
    	char str[] = "button pressed \r\n";
    	HAL_UART_Transmit(&huart2, (const uint8_t*) str, strlen(str), HAL_MAX_DELAY);

    	button_flag = 1;

    }
}

int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}

static FRESULT sd_result;
static FATFS sdCard;
static FIL wavFile;
static uint8_t first_time = 0;
static uint32_t wav_file_size;
void sd_card_init(void){
	sd_result = f_mount(&sdCard, "", 1);

	if (sd_result != 0){
		printf("error in mounting sd card %d \r\n", sd_result);
	}
	else printf("success in mounting sd card\r\n");
//
//	uint8_t file_name[] = "test.txt";
//	uint8_t temp_number;
//	uint8_t test_text[] = "hello friends\r\n";
//
//	sd_result = f_open(&testFile, (void*)file_name, FA_WRITE | FA_CREATE_ALWAYS);
//	if (sd_result != 0){
//		printf("error in creating file: %d \r\n", sd_result);
//		while(1);
//	}
//	else printf("succeeded in opening file \r\n");
//
//	sd_result = f_write(&testFile, (void*)test_text, (UINT)sizeof(test_text), (UINT)&temp_number);
//
//	if (sd_result != 0){
//		printf("error writing to the file \r\n");
//		while (1);
//	}
//	else printf("successfully wrote to file \r\n");


}


typedef struct {
    char     ChunkID[4];       // "RIFF"
    uint32_t ChunkSize;        // 36 + Subchunk2Size
    char     Format[4];        // "WAVE"

    char     Subchunk1ID[4];   // "fmt "
    uint32_t Subchunk1Size;    // 16 for PCM
    uint16_t AudioFormat;      // 1 for PCM
    uint16_t NumChannels;      // 1 or 2
    uint32_t SampleRate;       // e.g., 32000
    uint32_t ByteRate;         // SampleRate * NumChannels * BitsPerSample/8
    uint16_t BlockAlign;       // NumChannels * BitsPerSample/8
    uint16_t BitsPerSample;    // 8 or 16

    char     Subchunk2ID[4];   // "data"
    uint32_t Subchunk2Size;    // NumSamples * NumChannels * BitsPerSample/8
} WAVHeader;
//  0 -  3  -> "RIFF"                                  {0x52, 0x49, 0x46, 0x46}
//  4 -  7  -> size of the file in bytes               {data_section size + 36}
//  8 - 11  -> File type header, "WAVE"                {0x57, 0x41, 0x56, 0x45}
// 12 - 15  -> "fmt "                                  {0x66, 0x6d, 0x74, 0x20}
// 16 - 19  -> Length of format data                   16                  {0x10, 0x00, 0x00, 0x00}
// 20 - 21  -> type of format, pcm is                  1                   {0x01, 0x00}
// 22 - 23  -> number of channels                      2                   {0x02, 0x00}
// 24 - 27  -> sample rate,                            32 kHz              {0x80, 0x7d, 0x00, 0x00}
// 28 - 31  -> sample rate x bps x channels            19200               {0x00, 0xf4, 0x01, 0x00}
// 32 - 33  -> bps * channels                          4                   {0x04, 0x00}
// 34 - 35  -> bits per sample                         16                  {0x10, 0x00}
// 36 - 39  -> "data"                                  {0x64, 0x61, 0x74, 0x61}
// 40 - 43  -> size of the data section                {data section size}

static uint8_t wav_file_header[44] = {
    0x52, 0x49, 0x46, 0x46, 0xa4, 0xa9, 0x03, 0x00,
    0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20,
    0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x80, 0x7d, 0x00, 0x00, 0x00, 0xf4, 0x01, 0x00,
    0x04, 0x00, 0x10, 0x00, 0x64, 0x61, 0x74, 0x61,
    0x80, 0xa9, 0x03, 0x00
};


void start_recording(uint32_t frequency){
	// wave file format

	// 44 file header
	// sampling rate, resolution, number of bytes, etc, number of channels

	static char file_name[] = "w.wav";
	static uint8_t file_counter = 1;
	int file_number_digits = file_counter;


	uint32_t byte_rate = frequency * 2 * 2;
	wav_file_header[24] = (uint8_t)frequency;
	wav_file_header[25] = (uint8_t)(frequency >> 8);
	wav_file_header[26] = (uint8_t)(frequency >> 16);
	wav_file_header[27] = (uint8_t)(frequency >> 24);

	wav_file_header[28] = (uint8_t)byte_rate;
	wav_file_header[29] = (uint8_t)(byte_rate >> 8);
	wav_file_header[30] = (uint8_t)(byte_rate >> 16);
	wav_file_header[31] = (uint8_t)(byte_rate >> 24);


	// defining a wave file name
//	file_name[4] = file_number_digits % 10 + 48;
//	file_number_digits /= 10;
//	file_name[3] = file_number_digits % 10 + 48;
//	file_number_digits /= 10;
//	file_name[2] = file_number_digits % 10 + 48;
	printf("file name %s \n", file_name);
	file_counter++;


	// creating a file
	sd_result = f_open(&wavFile, file_name, FA_WRITE | FA_CREATE_ALWAYS);
	if (sd_result != 0)
	{
	    printf("error in creating a file: %d \n", sd_result);
	    while(1);
	}
	else
	{
	    printf("succeeded in opening a file \n");
	}

	wav_file_size = 0;

}


void write2wave_file(uint8_t *data, uint16_t data_size){
	uint16_t temp_number;
	printf("w\n");


	// change header.
	if (first_time == 0)
	{
	    for (int i = 0; i < 44; i++)
	    {
	        *(data + i) = wav_file_header[i];
	    }
	    first_time = 1;
	}


	sd_result = f_write(&wavFile, (void *)data, data_size, (UINT*)&temp_number);
	if (sd_result != 0)
	{
	    printf("error in writing to the file: %d \n", sd_result);
	    while(1);
	}
	wav_file_size += data_size;

}


void stop_recording(void){

	uint16_t temp_number;
	// updating data size sector
	wav_file_size -= 8;	// subtract 8 because don't consider first 8 bytes as part of the file
	wav_file_header[4]  = (uint8_t)wav_file_size;
	wav_file_header[5]  = (uint8_t)(wav_file_size >> 8);
	wav_file_header[6]  = (uint8_t)(wav_file_size >> 16);
	wav_file_header[7]  = (uint8_t)(wav_file_size >> 24);
	wav_file_size -= 36;	// subtract 36 because we have 36 bytes of header before data
	wav_file_header[40] = (uint8_t)wav_file_size;
	wav_file_header[41] = (uint8_t)(wav_file_size >> 8);
	wav_file_header[42] = (uint8_t)(wav_file_size >> 16);
	wav_file_header[43] = (uint8_t)(wav_file_size >> 24);


	// moving to the beginning of the file to update the file format
	f_lseek(&wavFile, 0);
	f_write(&wavFile, (void *)wav_file_header, sizeof(wav_file_header), (UINT*)&temp_number);
	if (sd_result != 0)
	{
	    printf("error in updating the first sector: %d \n", sd_result);
	    while(1);
	}
	f_close(&wavFile);
	first_time = 0;
	printf("closed the file \n");


}






void testSDCard(void){

	  printf("\r\n~ SD card demo by kiwih ~\r\n\r\n");

	  HAL_Delay(1000); //a short delay is important to let the SD card settle

	  //some variables for FatFs
	  FATFS FatFs; 	//Fatfs handle
	  FIL fil; 		//File handle
	  FRESULT fres; //Result after operations

	  //Open the file system
	  fres = f_mount(&FatFs, "", 1); //1=mount now
	  if (fres != FR_OK) {
		printf("f_mount error (%i)\r\n", fres);
		while(1);
	  }

	  //Let's get some statistics from the SD card
	  DWORD free_clusters, free_sectors, total_sectors;

	  FATFS* getFreeFs;

	  fres = f_getfree("", &free_clusters, &getFreeFs);
	  if (fres != FR_OK) {
		printf("f_getfree error (%i)\r\n", fres);
		while(1);
	  }

	  //Formula comes from ChaN's documentation
	  total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
	  free_sectors = free_clusters * getFreeFs->csize;

	  printf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

	  //Now let's try to open file "test.txt"
	  fres = f_open(&fil, "write.txt", FA_READ);
	  if (fres != FR_OK) {
		printf("f_open error (%i)\r\n", fres);
		while(1);
	  }
	  printf("I was able to open 'write.txt' for reading!\r\n");

	  //Read 30 bytes from "test.txt" on the SD card
	  BYTE readBuf[30];

	  //We can either use f_read OR f_gets to get data out of files
	  //f_gets is a wrapper on f_read that does some string formatting for us
	  TCHAR* rres = f_gets((TCHAR*)readBuf, 30, &fil);
	  if(rres != 0) {
		printf("Read string from 'write.txt' contents: %s\r\n", readBuf);
	  } else {
		printf("f_gets error (%i)\r\n", fres);
	  }

	  //Be a tidy kiwi - don't forget to close your file!
	  f_close(&fil);

	  //Now let's try and write a file "write.txt"
	  fres = f_open(&fil, "write.txt", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
	  if(fres == FR_OK) {
		printf("I was able to open 'write.txt' for writing\r\n");
	  } else {
		printf("f_open error (%i)\r\n", fres);
	  }

	  //Copy in a string
	  strncpy((char*)readBuf, "a new file is made!", 19);
	  UINT bytesWrote;
	  fres = f_write(&fil, readBuf, 19, &bytesWrote);
	  if(fres == FR_OK) {
		printf("Wrote %i bytes to 'write.txt'!\r\n", bytesWrote);
	  } else {
		printf("f_write error (%i)\r\n", fres);
	  }

	  //Be a tidy kiwi - don't forget to close your file!
	  f_close(&fil);

	  //We're done, so de-mount the drive
	  f_mount(NULL, "", 0);

	  printf("DONE! \r\n");

}


void test_speaker(void){
	  uint16_t test_buf[1000];
	  for (int i = 0; i < 1000; ++i) test_buf[i] = (i % 100) * 300; // simple ramp

	  HAL_StatusTypeDef result;

	  while (1){

		  result = HAL_I2S_Transmit(&hi2s2, test_buf, 1000, HAL_MAX_DELAY);
		  HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);

		  if (result == HAL_OK){
			  printf("I2S was okay \r\n");
		  }

		  else{
			  printf("failed I2S communication\r\n");
		  }
		  HAL_Delay(1000);
	  }
}

void SDcardPlaySetup(void){

	  fresult = f_mount(&fatfs, "", 1);

	  if (fresult != FR_OK){
		  // do something
		  printf("there was an error with the mounting");

	  }
	  else{
		  printf("successfully mounted\r\n");
	  }

	  fresult = f_open(&fil, "W.WAV", FA_OPEN_EXISTING | FA_READ);
//	  fresult = f_open(&fil, "a.wav", FA_OPEN_EXISTING | FA_READ);
	  if (fresult != FR_OK){
		  // do something
		  printf("could not read file, fresult is %d \r\n", fresult);
		  while (1);
	  }
	  else{
		  printf("successfully opened file!\r\n");
	  }

	  // added for playing back voice recording
	  printf("-----------------------------\r\n");
	  FILINFO finfo;
	  FRESULT res = f_stat("W.WAV", &finfo);
//	  FRESULT res = f_stat("a.wav", &finfo);
	  if (res == FR_OK) {
	      printf("Size: %lu bytes\r\n", finfo.fsize);
	  } else {
	      printf("f_stat failed: %d\r\n", res);
	  }

	  WAVHeader header;
	  UINT br;

	  f_lseek(&fil, 0); // Go to start
	  if (f_read(&fil, &header, sizeof(WAVHeader), &br) != FR_OK){
		  printf("failed to read into header\r\n");
		  while(1);
	  }


	  printf("ChunkID: %.4s\r\n", header.ChunkID);
	  printf("ChunkSize: %lu\r\n", header.ChunkSize);
	  printf("Format: %.4s\r\n", header.Format);

	  printf("Subchunk1ID: %.4s\r\n", header.Subchunk1ID);
	  printf("Subchunk1Size: %lu\r\n", header.Subchunk1Size);
	  printf("AudioFormat: %u\r\n", header.AudioFormat);
	  printf("NumChannels: %u\r\n", header.NumChannels);
	  printf("SampleRate: %lu\r\n", header.SampleRate);
	  printf("ByteRate: %lu\r\n", header.ByteRate);
	  printf("BlockAlign: %u\r\n", header.BlockAlign);
	  printf("BitsPerSample: %u\r\n", header.BitsPerSample);

	  printf("Subchunk2ID: %.4s\r\n", header.Subchunk2ID);
	  printf("Subchunk2Size: %lu\r\n", header.Subchunk2Size);



	  printf("seeking file size......\r\n");
	  fresult = f_lseek(&fil, 40);
	  if (fresult != FR_OK) printf("there was an error seeking\r\n");



	  fresult = f_read(&fil, &recording_size, 4, (UINT *)fread_size);
	  if (fresult != FR_OK){
		  // do something
		  printf("could not read file, fresult is %d \r\n", fresult);
		  while (1);
	  }
	  else{
		  printf("successfully read file!\r\n");
		  printf("the recording size is: %lu \r\n", recording_size);
	  }
	  printf("-----------------------------------\r\n");
	  // end of voice recording code


	  // from playback video, I think this was done wrong
//	  fresult = f_lseek(&fil, 44);
//	  if (fresult != FR_OK){
//		  // do something
//		  printf("could not seek file, fresult is %d \r\n", fresult);
//		  while (1);
//	  }
//	  else{
//		  printf("successfully seek within file!\r\n");
//	  }
//
//	  fresult = f_read(&fil, &recording_size, 4, (UINT *)fread_size);
//	  if (fresult != FR_OK){
//		  // do something
//		  printf("could not read file, fresult is %d \r\n", fresult);
//		  while (1);
//	  }
//	  else{
//		  printf("successfully read file!\r\n");
//		  printf("the recording size is: %lu \r\n", recording_size);
//	  }



	  fresult = f_lseek(&fil, 44);
	  if (fresult != FR_OK){
		  // do something
		  printf("could not seek file, fresult is %d \r\n", fresult);
		  while (1);
	  }
	  else{
		  printf("successfully seek within file!\r\n");
	  }
	  fresult = f_read(&fil, samples, 64000, (UINT *)fread_size);
	  if (fresult != FR_OK){
		  // do something
		  printf("could read file part 2, fresult is %d \r\n", fresult);
		  while (1);
	  }
	  else{
		  printf("successfully read file part 2!\r\n");
	  }

	  recording_size /= 2;

	  printf("============== done file operations!\r\n");

	  HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t *)samples, 32000);
}




void handleSDCardPlayback(void){
    // original video code
  if (callback_result == FULL_COMPLETED){
	  UINT bytesRead = 0;
	  // finsihed transmitting I2S out of second half of array
	  // so ready data into second half of array

//		  f_read(&fil, &samples[16000], 32000, (UINT *) fread_size);
	  f_read(&fil, &samples[16000], 32000, bytesRead);
	  played_size += 32000;
	  printf("%u bytes, callback result full completed -> unknown \r\n", bytesRead);
	  callback_result = UNKNOWN;
  }

  if (callback_result == HALF_COMPLETED){
	  UINT bytesRead = 0;
	  // finish transmitting I2S out of first half of array
	  // so read data into first half of array
	  f_read(&fil, &samples[0], 32000, bytesRead);
//		  f_read(&fil, &samples[0], 32000, (UINT *) fread_size);
	  printf("%u bytes read, callback result half completed -> unknown \r\n", bytesRead);
	  callback_result = UNKNOWN;
  }


  if (played_size >= recording_size){
	  HAL_I2S_DMAStop(&hi2s2);
	  printf("done DMA transfer \r\n");
	  HAL_Delay(10000);
  }

}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  MX_FATFS_Init();
  MX_I2S2_Init();
  MX_I2S3_Init();
  /* USER CODE BEGIN 2 */

  HAL_Delay(500);
  sd_card_init();

  printf("start...\r\n");
//  testSDCard();


  if (playback) SDcardPlaySetup();



  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {


	  if (!playback){
		  if (button_flag){
			  if (start_stop_recording){

				  HAL_I2S_DMAStop(&hi2s3);
				  start_stop_recording = 0;
				  stop_recording();
				  printf("stop recording\r\n");
			  }
			  else {
				  // initial value is 0, so need to start recording at the first press
				  start_recording(I2S_AUDIOFREQ_32K);
				  start_stop_recording = 1;
				  printf("start recording\r\n");


				  HAL_I2S_Receive_DMA(&hi2s3, (uint16_t*) data_i2s, sizeof(data_i2s)/2); // divide by 2 to get number of samples

			  }

			  button_flag = 0;
		  }

	  }


	  if (playback) handleSDCardPlayback();

//	  printf("Sample: %d\r\n", sample_i2s);
//	  HAL_Delay(10);




    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	if (!playback){
		  // when first half of buffer is full, write first half to file
		  if (start_stop_recording == 1 && half_i2s == 1){
			  write2wave_file(  ((uint8_t *) data_i2s) , WAV_WRITE_SAMPLE_COUNT);
			  half_i2s = 0;
		  }

		  // when second half is full, write second half to file
		  if (start_stop_recording == 1 && full_i2s == 1){
			  write2wave_file(  ((uint8_t *) data_i2s) + WAV_WRITE_SAMPLE_COUNT, WAV_WRITE_SAMPLE_COUNT);
			  full_i2s = 0;
		  }
	}

  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2S;
  PeriphClkInitStruct.PLLI2S.PLLI2SN = 96;
  PeriphClkInitStruct.PLLI2S.PLLI2SR = 2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_16B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_16K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s2.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_ENABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * @brief I2S3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S3_Init(void)
{

  /* USER CODE BEGIN I2S3_Init 0 */

  /* USER CODE END I2S3_Init 0 */

  /* USER CODE BEGIN I2S3_Init 1 */

  /* USER CODE END I2S3_Init 1 */
  hi2s3.Instance = SPI3;
  hi2s3.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s3.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s3.Init.DataFormat = I2S_DATAFORMAT_16B_EXTENDED;
  hi2s3.Init.MCLKOutput = I2S_MCLKOUTPUT_DISABLE;
  hi2s3.Init.AudioFreq = I2S_AUDIOFREQ_32K;
  hi2s3.Init.CPOL = I2S_CPOL_LOW;
  hi2s3.Init.ClockSource = I2S_CLOCK_PLL;
  hi2s3.Init.FullDuplexMode = I2S_FULLDUPLEXMODE_DISABLE;
  if (HAL_I2S_Init(&hi2s3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S3_Init 2 */

  /* USER CODE END I2S3_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD3_GPIO_Port, LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BLUE_PB_Pin */
  GPIO_InitStruct.Pin = BLUE_PB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BLUE_PB_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_CS_Pin LD3_Pin */
  GPIO_InitStruct.Pin = SPI1_CS_Pin|LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef * hi2s){
	callback_result = HALF_COMPLETED;
	printf("HALF callback\r\n");


}



void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef * hi2s){

	// whenever called, 32000 samples already got played
	callback_result = FULL_COMPLETED;
	played_size += 32000;
	printf("FULL callback\r\n");

}


void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef * hi2s){
	sample_i2s = data_i2s[1];
	full_i2s = 1;
}

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef * hi2s){
	half_i2s = 1;
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
