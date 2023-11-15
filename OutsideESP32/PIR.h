#include <Arduino.h>

#define PIR_INPUT_PIN 18


void init_pir(); // Set up PIR sensor
void IRAM_ATTR on_pir_motion_detected();
bool is_motion_detected(); // Returns whether or not the PIR sensor detects movement