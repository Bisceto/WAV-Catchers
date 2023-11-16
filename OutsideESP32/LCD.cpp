#include "LCD.h"

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
SemaphoreHandle_t lcd_semaphore = xSemaphoreCreateMutex();
hw_timer_t *clear_timer = NULL;
bool waiting_to_clear_display = false;

void IRAM_ATTR on_clear_timer_finished()
{
  // clear display
  waiting_to_clear_display = true;

  // Disable timer
  timerAlarmDisable(clear_timer);
}

void init_lcd()
{
  assert(lcd_semaphore);

  // take control of display
  xSemaphoreTake(lcd_semaphore, portMAX_DELAY);

  lcd.init();
  lcd.clear();                   
  lcd.backlight();

  clear_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(clear_timer, &on_clear_timer_finished, true);

  // give up ownership of display
  xSemaphoreGive(lcd_semaphore);
}

void start_clear_timer()
{
  waiting_to_clear_display = false;
  timerAlarmWrite(clear_timer, 15000000, true);
  timerAlarmEnable(clear_timer);
}

void printLCD(const char *message)
{
  // take control of display
  xSemaphoreTake(lcd_semaphore, portMAX_DELAY);

  // clear screen
  lcd.clear();

  // turn on screen
  lcd.backlight();
  
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

  // start clear timer
  start_clear_timer();

  // give up ownership of display
  xSemaphoreGive(lcd_semaphore);
}

void disable_lcd()
{
  // take control of display
  xSemaphoreTake(lcd_semaphore, portMAX_DELAY);

  // clear screen
  lcd.clear();

  // turn off screen
  lcd.noBacklight();

  // give up ownership of display
  xSemaphoreGive(lcd_semaphore);
}