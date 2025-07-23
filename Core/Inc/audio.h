/*
 * audio.h
 *
 *  Created on: Jul 20, 2025
 *      Author: ilan1
 */

#ifndef INC_AUDIO_H_
#define INC_AUDIO_H_


#include "stm32f4xx_hal.h"

#define WAV_WRITE_SAMPLE_COUNT 1024 //number of mono samples per DMA cycle
//#define WAV_READ_SAMPLE_COUNT 1024
#define WAV_READ_SAMPLE_COUNT 3200

typedef enum {
	UNKNOWN,
	HALF_COMPLETED,
	FULL_COMPLETED
}  CallBack_Result_t;


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


void testSDCard(void);
void test_speaker(void);
void testMicData(void);
void disableSpeakerCrackle(void);

void sd_card_init(void);
void start_recording(uint32_t frequency);
void write2wave_file(uint8_t *data, uint16_t data_size);
void stop_recording(void);
void handle_recording_main(void);


void SDcardPlaySetup(uint8_t playSong);
void handleSDCardPlayback(void);


#endif /* INC_AUDIO_H_ */
