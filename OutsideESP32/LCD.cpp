#include "LCD.h"

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
SemaphoreHandle_t lcd_semaphore = xSemaphoreCreateMutex();

void init_lcd()
{
  assert(lcd_semaphore);
  lcd.init();
  lcd.clear();                   
  lcd.backlight();
}

void printLCD(const char *message)
{
  // take control of display
  xSemaphoreTake(lcd_semaphore, portMAX_DELAY);

  // clear screen
  lcd.clear();
  
  // message into 2 substrings by linebreak
  String readString = String(message);
  int linebreak_index = readString.indexOf('\n');

  // top line
  String top = readString.substring(0, linebreak_index);
  lcd.setCursor(0, 0);
  lcd.print(top);

  // bottom line
  if (linebreak_index != -1 && linebreak_index < readString.length() - 1) 
  {
    String bottom = readString.substring(linebreak_index + 1, readString.length());
    lcd.setCursor(0, 1);
    lcd.print(bottom);
  }

  // give up ownership of display
  xSemaphoreGive(lcd_semaphore);
}