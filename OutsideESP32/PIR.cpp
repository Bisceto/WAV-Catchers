#include "PIR.h"

int pir_state = LOW;

bool is_motion_detected()
{
  int val = digitalRead(PIR_INPUT_PIN);
  if (val == HIGH) {
    if (pir_state == LOW) 
    {
      pir_state = HIGH;
      return true;
    }
  } else {
    if (pir_state == HIGH) 
    {
      pir_state = LOW;
      return false;
    }
  }
  
  return false;
}