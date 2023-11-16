// Modified from https://github.com/MhageGH/esp32_SoundRecorder/tree/master
// by MhageGH

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2s.h>
#include "esp_system.h"

#define I2S_WS            15 //14 
#define I2S_SD            13
#define I2S_SCK           2  //12
#define I2S_PORT          I2S_NUM_0
#define I2S_SAMPLE_RATE   (16000)
#define I2S_SAMPLE_BITS   (16)
#define I2S_READ_LEN      (16 * 1024)
#define RECORD_TIME       (5)     //Seconds
#define I2S_CHANNEL_NUM   (1)     // How many i2s channels are being used to record
#define WAV_HEADER_SIZE   (44)    // Size of .wav file header in bytes
#define WAV_RECORD_SIZE   ((I2S_CHANNEL_NUM * I2S_SAMPLE_RATE * I2S_SAMPLE_BITS / 8) * RECORD_TIME) // Divide by 8 to cahnge from bits to bytes

void i2s_init();
void i2s_adc_data_scale(uint8_t * d_buff, uint8_t* s_buff, uint32_t len);
void create_wav_header(byte* header, int wav_data_size);
