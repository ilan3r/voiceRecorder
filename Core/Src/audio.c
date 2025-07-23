/*
 * audio.c
 *
 *  Created on: Jul 20, 2025
 *      Author: ilan1
 *
 *      BE CAREFUL:
 *      - make sure I2S data wires are connected proprly
 *
 *
 *      TODO:
 *      - when playing song, at the end, it doesn't stop when it should
 *      - when playing recording, cuts out about a second too early
 *
 */

#include "audio.h"
#include "string.h"
#include "fatfs.h"



extern I2S_HandleTypeDef hi2s2;
extern I2S_HandleTypeDef hi2s3;
extern DMA_HandleTypeDef hdma_spi2_tx;
extern DMA_HandleTypeDef hdma_spi3_rx;


FATFS fatfs;
FIL fil;
FRESULT fresult;

// reading variables
int16_t samples[WAV_READ_SAMPLE_COUNT];
uint32_t fread_size = 0;
uint32_t recording_size = 0;
uint32_t played_size = 0;

uint8_t playback = 1;
volatile CallBack_Result_t callback_result = UNKNOWN;
char playbackFile[] = "WW.WAV";

// recording variables
volatile uint8_t half_i2s, full_i2s;
volatile uint8_t button_flag, start_stop_recording;
int16_t data_i2s[WAV_WRITE_SAMPLE_COUNT*2];
volatile int16_t mono_sample_i2s[WAV_WRITE_SAMPLE_COUNT];


static FRESULT sd_result;
static FATFS sdCard;
static FIL wavFile;
static uint8_t first_time = 0;
static uint32_t wav_file_size;




static uint8_t wav_file_header[44] = {
    0x52, 0x49, 0x46, 0x46,  // "RIFF"                      — ChunkID
    0xa4, 0xa9, 0x03, 0x00,  // ChunkSize = 0x03A9A4 (240164) = file size - 8
    0x57, 0x41, 0x56, 0x45,  // "WAVE"                      — Format
    0x66, 0x6d, 0x74, 0x20,  // "fmt "                      — Subchunk1ID
    0x10, 0x00, 0x00, 0x00,  // Subchunk1Size = 16          — PCM header size
    0x01, 0x00,              // AudioFormat = 1             — PCM
    0x01, 0x00,              // NumChannels = 1             — Mono
    0x80, 0x7d, 0x00, 0x00,  // SampleRate = 32000          — 0x7D80
    0x00, 0xfa, 0x00, 0x00,  // ByteRate = 128000           — should be 64000 for mono!
    0x02, 0x00,              // BlockAlign = 2              — 2 bytes per sample (16-bit mono)
    0x10, 0x00,              // BitsPerSample = 16
    0x64, 0x61, 0x74, 0x61,  // "data"                      — Subchunk2ID
    0x80, 0xa9, 0x03, 0x00   // Subchunk2Size = 0x03A980 (240128) = data size in bytes
};


/////////////////////// testing functions for hardware
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

void testMicData(void){
	  printf("Sample: %d\r\n", data_i2s[0]);
	  HAL_Delay(10);
}

void SDListFiles(void){
    // Mount the file system
	  //some variables for FatFs
    FATFS FatFs; 	//Fatfs handle
    FIL fil; 		//File handle
	FRESULT res;
	DIR dir;
	FILINFO fno;
    res = f_mount(&FatFs, "", 1);  // "" means default drive, 1 means mount immediately
    if (res != FR_OK) {
        printf("Failed to mount filesystem. Error code: %d\r\n", res);
        return;
    }

    // Open root directory
    res = f_opendir(&dir, "/");
    if (res != FR_OK) {
        printf("Failed to open root directory. Error: %d\r\n", res);
        return;
    }

    // Read directory entries
    while (1) {
        res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == 0) break; // Stop on error or end of directory

        if (fno.fattrib & AM_DIR) {
            printf("Dir : %s\r\n", fno.fname);
        } else {
            printf("File: %s\r\n", fno.fname);
        }
    }

    f_closedir(&dir);
    f_mount(NULL, "", 0);

    printf("\n closed directory, done listing all files \r\n");

}

//////////////////////// recording data functions

void sd_card_init(void){
	sd_result = f_mount(&sdCard, "", 1);

	if (sd_result != 0){
		printf("error in mounting sd card %d \r\n", sd_result);
	}
	else printf("success in mounting sd card\r\n");

}


void start_recording(uint32_t frequency){
	// wave file format

	// 44 file header
	// sampling rate, resolution, number of bytes, etc, number of channels

	static char file_name[] = "ww.wav";
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

void handle_recording_main(void){
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
	  // when first half of buffer is full, write first half to file
	  if (start_stop_recording == 1 && half_i2s == 1){

		  write2wave_file(  (uint8_t *) mono_sample_i2s, WAV_WRITE_SAMPLE_COUNT);
		  half_i2s = 0;
	  }

	  // when second half is full, write second half to file
	  if (start_stop_recording == 1 && full_i2s == 1){
		  write2wave_file(  (uint8_t *) &mono_sample_i2s[WAV_WRITE_SAMPLE_COUNT/2], WAV_WRITE_SAMPLE_COUNT);
		  full_i2s = 0;
	  }
}


////////////////////////////// playback functions

void SDcardPlaySetup(uint8_t playSong){

	  fresult = f_mount(&fatfs, "", 1);

	  if (fresult != FR_OK){
		  // do something
		  printf("there was an error with the mounting");

	  }
	  else{
		  printf("successfully mounted\r\n");
	  }


	  if (playSong) fresult = f_open(&fil, "a.wav", FA_OPEN_EXISTING | FA_READ);
	  else  fresult = f_open(&fil, playbackFile, FA_OPEN_EXISTING | FA_READ);
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
	  FRESULT res;
	  if (playSong) res = f_stat("a.wav", &finfo);
	  else res = f_stat(playbackFile, &finfo);
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

	  if (playSong){
		  fresult = f_lseek(&fil, 44);
		  if (fresult != FR_OK){
			  // do something
			  printf("could not seek file, fresult is %d \r\n", fresult);
			  while (1);
		  }
		  else{
			  printf("successfully seek within file!\r\n");
		  }

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

	  }

	  // from playback video, I think this was done wrong



	  fresult = f_lseek(&fil, 44);
	  if (fresult != FR_OK){
		  // do something
		  printf("could not seek file, fresult is %d \r\n", fresult);
		  while (1);
	  }
	  else{
		  printf("successfully seek within file!\r\n");
	  }
	  fresult = f_read(&fil, samples, WAV_READ_SAMPLE_COUNT*2, (UINT *)fread_size);
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

	  HAL_I2S_Transmit_DMA(&hi2s2, (uint16_t *)samples, WAV_READ_SAMPLE_COUNT);
}


void handleSDCardPlayback(void){
    // original video code
  if (callback_result == FULL_COMPLETED){
	  UINT bytesRead = 0;
	  // finsihed transmitting I2S out of second half of array
	  // so ready data into second half of array

//		  f_read(&fil, &samples[16000], 32000, (UINT *) fread_size);
	  f_read(&fil, &samples[WAV_READ_SAMPLE_COUNT/2], WAV_READ_SAMPLE_COUNT, bytesRead);
	  played_size += WAV_READ_SAMPLE_COUNT;
	  printf("%u bytes, callback result full completed -> unknown \r\n", bytesRead);
	  callback_result = UNKNOWN;
  }

  if (callback_result == HALF_COMPLETED){
	  UINT bytesRead = 0;
	  // finish transmitting I2S out of first half of array
	  // so read data into first half of array
	  f_read(&fil, &samples[0], WAV_READ_SAMPLE_COUNT, bytesRead);
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




