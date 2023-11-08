// Modified from https://github.com/MhageGH/esp32_SoundRecorder/tree/master
// by MhageGH

#include <Arduino.h>

// 16bit, monoral, 44100Hz,  linear PCM
void create_wav_header(byte* header, int wav_data_size);  // size of header is 44
