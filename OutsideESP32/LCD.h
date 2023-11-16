// Vcc - 5V
// SDA - Pin 21
// SCL - Pin 22
#include <LiquidCrystal_I2C.h>
#include <mutex>

#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

void IRAM_ATTR on_clear_timer_finished();
void init_lcd(); // Initialises the LCD display
void start_clear_timer();
void printLCD(const char *message); // prints a string on the LCD display
void disable_lcd();