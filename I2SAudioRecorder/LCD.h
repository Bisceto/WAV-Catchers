// Modified from https://github.com/MhageGH/esp32_SoundRecorder/tree/master
// by MhageGH


#include <LiquidCrystal_I2C.h>
#include <mutex>

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

void init_lcd(); // Initialises the LCD display
void printLCD(const char *message); // prints a string on the LCD display