// Modified from https://github.com/MhageGH/esp32_SoundRecorder/tree/master
// by MhageGH

#include "Arduino.h"
#include <FS.h>
#include "Wav.h"
#include "I2S.h"

#include <WiFi.h>
#include "CustomESP32MQTTClient.h"

#include <LiquidCrystal_I2C.h>

#define CONNECTION_TIMEOUT 20

// MQTT Settings
const char *ssid = "AndroidAP3542";
const char *pass = "wav_catchers";

char *server = "mqtt://192.168.15.242:1883"; // "mqtt://<IP Addr of MQTT BROKER>:<Port Number>"

char *startRecordingTopic = "sensors/microphone/recording_started";     // Published when recording is started
char *addAudioSnippetTopic = "sensors/microphone/snippet";              // Published with byte array of audio data while recording
char *endRecordingTopic = "sensors/microphone/recording_finished";      // Published when recording has ended

char *lcdDisplayTopic = "actuators/lcd/display_message";                // Subscribe for when server request to display a message on LCD

ESP32MQTTClient mqttClient; // all params are set later

// I2S Settings
byte header[WAV_HEADER_SIZE];

// LCD Settings
LiquidCrystal_I2C lcd(0x27, 16, 2);  


void setup() {

  // Initialise Serial Output
  Serial.begin(115200);

  // Initialise LCD
  lcd.init();
  lcd.clear();                   
  lcd.backlight();

  // Setup MQTT
  log_i();
  log_i("setup, ESP.getSdkVersion(): ");
  log_i("%s", ESP.getSdkVersion());

  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(server);
  mqttClient.enableLastWillMessage("lwt", "I am going offline");
  mqttClient.setKeepAlive(30);

  // Connect to WiFi
  WiFi.begin(ssid, pass);
  int timeout_counter = 0;

  Serial.println("Connecting...");
  lcd.print("Connecting...");
  if (WiFi.status() != WL_CONNECTED) {
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print('.');
      delay(500);

      if (timeout_counter++ > CONNECTION_TIMEOUT) {
        Serial.println("\nRestarting due to Connection Timeout!");
        ESP.restart();
      }
    }
  }
  lcd.print("Connected to\nWiFi!");
  Serial.println("Connected to WiFi!");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());

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
  display_message("Recording...");

  //
  size_t published_byte_count = 0; // excluding header
  size_t bytes_read = 0;

  char* read_buffer = (char*) calloc(I2S_READ_LEN, sizeof(char));
  uint8_t* write_buffer = (uint8_t*) calloc(I2S_READ_LEN, sizeof(char));

  // transmit header first
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
  display_message("Please wait...");

  // free memory
  free(read_buffer);
  free(write_buffer);

  // end task
  vTaskDelete(NULL);

}


void i2s_adc_data_scale(uint8_t * d_buff, uint8_t* s_buff, uint32_t len) 
{
    uint32_t j = 0;
    uint32_t dac_value = 0;
    for (int i = 0; i < len; i += 2) {
        dac_value = ((((uint16_t) (s_buff[i + 1] & 0xf) << 8) | ((s_buff[i + 0]))));
        d_buff[j++] = 0;
        d_buff[j++] = dac_value * 256 / 2048;
    }
}


void display_message(const char *message)
{
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
  if (linebreak_index != -1) 
  {
    String bottom = readString.substring(linebreak_index, readString.length());
    lcd.setCursor(0, 1);
    lcd.print(bottom);
  }
}


void loop() {
}


void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client)
{
    if (mqttClient.isMyTurn(client)) // can be omitted if only one client
    {
        mqttClient.subscribe(lcdDisplayTopic, [](const String &payload)
                             { display_message(payload.c_str()); });

        mqttClient.subscribe("bar/#", [](const String &topic, const String &payload)
                             { log_i("%s: %s", topic, payload.c_str()); });
    }
}


esp_err_t handleMQTT(esp_mqtt_event_handle_t event)
{
    mqttClient.onEventCallback(event);
    return ESP_OK;
}
