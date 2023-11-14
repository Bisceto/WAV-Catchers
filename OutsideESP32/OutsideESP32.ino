// Modified from https://github.com/MhageGH/esp32_SoundRecorder/tree/master
// by MhageGH

#include "Arduino.h"
#include <FS.h>
#include "I2S.h"
#include "LCD.h"
#include "PIR.h"

#include <WiFi.h>
#include "CustomESP32MQTTClient.h"
#include "esp_wpa2.h" //wpa2 library for connections to Enterprise networks

// State Machine
enum state{IDLE, RECORDING, AWAITING_RECORDING_RESULTS, LOCKOUT};
volatile enum state current_state = IDLE;

// LCD
extern bool waiting_to_clear_display;

// Push Button , LED
#define LED_PIN 25 // LED Pin
#define PB_PIN 4  // PushButton Pin
#define TDELAY 1000
volatile bool pressed = false;
volatile unsigned long pressedtime = 0;
volatile unsigned long enablePB = 0;
volatile unsigned long movedtime = 0;
volatile bool moved = false;
// MQTT Settings

#define WIFI_TIMEOUT 10 // Seconds
#define MQTT_TIMEOUT 10 // Seconds

#define NUS_NET_IDENTITY "nusstu\<nusnet id>"  //ie nusstu\e0123456
#define NUS_NET_USERNAME "<nusnet id>"
#define NUS_NET_PASSWORD "<nusnet password>"

// const char *ssid = "NUS_STU";
// const char *pass = "wav_catchers";

// char *server = "mqtt://172.31.120.177:1883"; // "mqtt://<IP Addr of MQTT BROKER>:<Port Number>"

const char *ssid = "POCOF5";
const char *pass = "qqqqqqqqq";

char *server = "mqtt://192.168.39.243:1883"; // "mqtt://<IP Addr of MQTT BROKER>:<Port Number>"

// publishing topics
char *startRecordingTopic = "sensors/microphone/recording_started";     // Published when recording is started
char *addAudioSnippetTopic = "sensors/microphone/snippet";              // Published with byte array of audio data while recording
char *endRecordingTopic = "sensors/microphone/recording_finished";      // Published when recording has ended
char *pirmotion = "sensors/motion";                                     // Published when PIR sensor triggers (sensitive)

// subscribe topics
char *lcdDisplayTopic = "actuators/lcd/display_message";                // Subscribe for when server request to display a message on LCD
char *wrongPasswordAttempt = "actuators/lcd/wrong_password_attempt";
char *correctPasswordAttempt = "actuators/lcd/correct_password_attempt";
char *resetAttempts = "outside_board/reset_attempts";
char *detectionCamera = "detection/camera";

ESP32MQTTClient mqttClient; // all params are set later


void IRAM_ATTR on_button_pressed() 
{
  // Only trigger a PB press after recording is done (additional 2sec buffer)
  // Only trigger a PB press if camera found person in frame
  if (millis() - pressedtime > RECORD_TIME * 1000 + 2000 && millis() - enablePB < 30000 ){
    pressedtime = millis();
    pressed = true;
  }
}

void IRAM_ATTR on_pir_motion_detected()
{
  if (millis() - movedtime >  5000){   // if moved, delay of 10s 
    moved = true;
    movedtime = millis();
  }
  
}

void init_pir()
{
  pinMode(PIR_INPUT_PIN, INPUT_PULLUP);
  attachInterrupt(PIR_INPUT_PIN, on_pir_motion_detected, RISING);
}

void setup() 
{
  // Initialise Serial Output
  Serial.begin(115200);

  // Initalise LED and push button
  pinMode(PB_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  attachInterrupt(PB_PIN, on_button_pressed, FALLING);

  // Initialise LCD
  init_lcd();

  // Initialise PIR
  init_pir();

  // Setup MQTT
  log_i();
  log_i("setup, ESP.getSdkVersion(): ");
  log_i("%s", ESP.getSdkVersion());

  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(server);
  mqttClient.enableLastWillMessage("lwt", "I am going offline");
  mqttClient.setKeepAlive(30);

  // Connect to WiFi delay(10);
  WiFi.disconnect(true);
  // WiFi.begin(ssid, WPA2_AUTH_PEAP, NUS_NET_IDENTITY, NUS_NET_USERNAME, NUS_NET_PASSWORD); // without CERTIFICATE you can comment out the test_root_ca on top. Seems like root CA is included?
 
  WiFi.begin(ssid, pass);
  int timeout_counter = 0;

  Serial.println("Connecting...");
  printLCD("Connecting...");

  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(WIFI_TIMEOUT * 100);

      if (timeout_counter++ > WIFI_TIMEOUT) {
        Serial.println("\nRestarting due to Connection Timeout!");
        ESP.restart();
      }
    }
  }

  Serial.println("Connected to WiFi!");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());
  printLCD("Connected to\nWiFi!");

  WiFi.setHostname("c3test");

  // Start MQTT
  mqttClient.loopStart();

  // Initialise I2S
  i2s_init();

  delay(2000); // DO NOT REMOVE, DOES NOT RECORD PROPERLY WIHTOUT THIS

}


void record_and_transmit_audio(void *param)
{
  // Read and Transmit

  // broadcast that recording has begun
  String startingPayload = String(WAV_RECORD_SIZE);
  mqttClient.publishFixedLength(startRecordingTopic, (char *) startingPayload.c_str(), startingPayload.length());
  printLCD("Recording...");

  //
  size_t published_byte_count = 0; // excluding header
  size_t bytes_read = 0;

  char* read_buffer = (char*) calloc(I2S_READ_LEN, sizeof(char));
  uint8_t* write_buffer = (uint8_t*) calloc(I2S_READ_LEN, sizeof(char));

  // transmit header first
  byte header[WAV_HEADER_SIZE];
  create_wav_header(header, WAV_RECORD_SIZE);
  mqttClient.publishFixedLength(addAudioSnippetTopic, (char *) header, WAV_HEADER_SIZE);

  // read garbage data
  i2s_read(I2S_PORT, (void*) read_buffer, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
  i2s_read(I2S_PORT, (void*) read_buffer, I2S_READ_LEN, &bytes_read, portMAX_DELAY);

  // start recording
  Serial.println(" *** Recording Start *** ");

  while (published_byte_count < WAV_RECORD_SIZE) {

    // read data from i2s bus
    i2s_read(I2S_PORT, (void*) read_buffer, I2S_READ_LEN, &bytes_read, portMAX_DELAY);
    
    // format data
    i2s_adc_data_scale(write_buffer, (uint8_t*) read_buffer, I2S_READ_LEN);

    // publish data
    mqttClient.publishFixedLength(addAudioSnippetTopic, (char *) write_buffer, I2S_READ_LEN);

    // update guard
    published_byte_count += bytes_read;

    // print progress
    Serial.print("Transmitted ");
    Serial.print(float(published_byte_count * 100) / WAV_RECORD_SIZE);
    Serial.println("%");
    
  }

  // finish recording
  String endingPayload = String(published_byte_count);
  mqttClient.publishFixedLength(endRecordingTopic, (char *) endingPayload.c_str(), endingPayload.length());
  Serial.println(" *** Recording Finished *** ");
  printLCD("Please wait...");

  // free memory
  free(read_buffer);
  free(write_buffer);

  // return to IDLE state
  current_state = AWAITING_RECORDING_RESULTS;

  // end task
  vTaskDelete(NULL);
}

void loop() {

  // FSM
  switch (current_state)
  {
    case IDLE:
      
      // Turn off LCD (ONLY when in idle / lockout state)
      if (waiting_to_clear_display) 
      {
        disable_lcd();
        waiting_to_clear_display = false;
      }

      // PIR
      if (is_motion_detected()) // change to prompt from CAM
      {
        printLCD("Press button to\nrecord password");
        if (moved) {
          moved = false;
          Serial.println("Motion detected");
          String msg_moved = "moved";
          mqttClient.publish(pirmotion, msg_moved);
        }
      }

      // Pushbutton
      if (pressed) { 
        current_state = RECORDING;
        xTaskCreate(toggleLED, "Toggle LED", 1024, NULL, 1, NULL);
        xTaskCreatePinnedToCore(record_and_transmit_audio, "record_and_transmit_audio", 4096, NULL, 1, NULL, 1);
        pressed = false;
      }
      delay(10);
      break;
    
    case RECORDING:

      break;

    case AWAITING_RECORDING_RESULTS:

      break;
    
    case LOCKOUT:
      
      // Turn off LCD (ONLY when in idle / lockout state)
      if (waiting_to_clear_display) 
      {
        disable_lcd();
        waiting_to_clear_display = false;
      }

      // PIR
      if (is_motion_detected())
      {
        printLCD("Wrong passwords\nIn Lockout");
      }

      // Pushbutton
      if (pressed) { 
        printLCD("No attempts\nremaining!");
        pressed = false;
      }
      delay(10);
      break;
  }
}

void toggleLED(void * arg){
  digitalWrite(LED_PIN, HIGH);
  vTaskDelay(RECORD_TIME * 1000); // LED lights up for recording time
  digitalWrite(LED_PIN, LOW);
  vTaskDelete(NULL);
}


void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client)
{
    if (mqttClient.isMyTurn(client)) // can be omitted if only one client
    {
        mqttClient.subscribe(lcdDisplayTopic, [](const String &payload)
                             { 
                              printLCD(payload.c_str());
                              current_state = IDLE;
                             }
                             );
        
        mqttClient.subscribe(correctPasswordAttempt, [](const String &payload)
                            {
                              printLCD("Correct Password\nWelcome");
                              current_state = IDLE;
                            }
                            );
        
        mqttClient.subscribe(wrongPasswordAttempt, [](const String &payload)
                            {
                              if (payload.toInt() <= 0) 
                              {
                                String str = "Wrong Password\nEntering Lockout";
                                printLCD(str.c_str());
                                current_state = LOCKOUT;
                              } 
                              else 
                              {
                                String str = "Wrong Password\n" + payload + " attempts left";
                                printLCD(str.c_str());
                                current_state = IDLE;
                              }
                            }
                            );
        
        mqttClient.subscribe(resetAttempts, [](const String &payload)
                            {
                              printLCD("Attempts Reset!");
                              current_state = IDLE;
                            }
                            );
        mqttClient.subscribe(detectionCamera, [](const String &payload)
                            {
                              enablePB = millis();
                            }
                            );
        mqttClient.subscribe("bar/#", [](const String &topic, const String &payload)
                             { log_i("%s: %s", topic, payload.c_str()); });
    }
}


esp_err_t handleMQTT(esp_mqtt_event_handle_t event)
{
    mqttClient.onEventCallback(event);
    return ESP_OK;
}