// Modified from https://github.com/MhageGH/esp32_SoundRecorder/tree/master
// by MhageGH

#include "Wav.h"

void create_wav_header(byte* header, int wav_file_size) {
  header[0] = 'R';
  header[1] = 'I';
  header[2] = 'F';
  header[3] = 'F';
  unsigned int file_size_minus8 = wav_file_size + 44 - 8;
  header[4] = (byte)(file_size_minus8 & 0xFF);
  header[5] = (byte)((file_size_minus8 >> 8) & 0xFF);
  header[6] = (byte)((file_size_minus8 >> 16) & 0xFF);
  header[7] = (byte)((file_size_minus8 >> 24) & 0xFF);
  header[8] = 'W';
  header[9] = 'A';
  header[10] = 'V';
  header[11] = 'E';
  header[12] = 'f';
  header[13] = 'm';
  header[14] = 't';
  header[15] = ' ';
  header[16] = 0x10;
  header[17] = 0x00;
  header[18] = 0x00;
  header[19] = 0x00;
  header[20] = 0x01;
  header[21] = 0x00;
  header[22] = 0x01;
  header[23] = 0x00;
  header[24] = 0x80;
  header[25] = 0x3E;
  header[26] = 0x00;
  header[27] = 0x00;
  header[28] = 0x00;
  header[29] = 0x7D;
  header[30] = 0x00;
  header[31] = 0x00;
  header[32] = 0x02;
  header[33] = 0x00;
  header[34] = 0x10;
  header[35] = 0x00;
  header[36] = 'd';
  header[37] = 'a';
  header[38] = 't';
  header[39] = 'a';
  header[40] = (byte)(file_size_minus8 & 0xFF);
  header[41] = (byte)((file_size_minus8 >> 8) & 0xFF);
  header[42] = (byte)((file_size_minus8 >> 16) & 0xFF);
  header[43] = (byte)((file_size_minus8 >> 24) & 0xFF);
}

