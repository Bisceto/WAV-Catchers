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

#define WIFI_TIMEOUT 10 // Seconds
#define MQTT_TIMEOUT 10 // Seconds

// MQTT Settings

#define NUS_NET_IDENTITY "nusstu\<nusnet-ID>"  //ie nusstu\e0123456
#define NUS_NET_USERNAME "<nusnet-ID>"
#define NUS_NET_PASSWORD "<nusnet-password>"

const char *ssid = "NUS_STU";
const char *pass = "wav_catchers";

char *server = "mqtt://172.31.123.112:1883"; // "mqtt://<IP Addr of MQTT BROKER>:<Port Number>"

// publishing topics
char *startRecordingTopic = "sensors/microphone/recording_started";     // Published when recording is started
char *addAudioSnippetTopic = "sensors/microphone/snippet";              // Published with byte array of audio data while recording
char *endRecordingTopic = "sensors/microphone/recording_finished";      // Published when recording has ended

// subscribe topics
char *lcdDisplayTopic = "actuators/lcd/display_message";                // Subscribe for when server request to display a message on LCD

ESP32MQTTClient mqttClient; // all params are set later


void setup() 
{
  // Initialise Serial Output
  Serial.begin(115200);

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
  WiFi.begin(ssid, WPA2_AUTH_PEAP, NUS_NET_IDENTITY, NUS_NET_USERNAME, NUS_NET_PASSWORD); // without CERTIFICATE you can comment out the test_root_ca on top. Seems like root CA is included?
 
  //WiFi.begin(ssid, pass);
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

  // Begin Recording
  xTaskCreatePinnedToCore(record_and_transmit_audio, "record_and_transmit_audio", 4096, NULL, 1, NULL, 1);
  
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

  // end task
  vTaskDelete(NULL);
}

void loop() {
  
  if (is_motion_detected())
  {
    printLCD("Motion Detected!");
  }

}


void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client)
{
    if (mqttClient.isMyTurn(client)) // can be omitted if only one client
    {
        mqttClient.subscribe(lcdDisplayTopic, [](const String &payload)
                             { printLCD(payload.c_str()); });

        mqttClient.subscribe("bar/#", [](const String &topic, const String &payload)
                             { log_i("%s: %s", topic, payload.c_str()); });
    }
}


esp_err_t handleMQTT(esp_mqtt_event_handle_t event)
{
    mqttClient.onEventCallback(event);
    return ESP_OK;
}
