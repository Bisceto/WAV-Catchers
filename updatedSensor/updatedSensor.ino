#include <WiFi.h>
#include "ESP32MQTTClient.h"
int inputPin = 12;    // choose the input pin (for PIR sensor)
int pirState = LOW;  // we start, assuming no motion detected
int val = 0;         // variable for reading the pin status

ESP32MQTTClient mqttClient;

const char *ssid = "Aa";
const char *pass = "12345678";

// Test Mosquitto server, see: https://test.mosquitto.org
char *server = "mqtt://172.20.10.2:1883";
char *subscribeTopic = "detection/camera";
char *publishTopic = "detection/sensor";


void setup() {
  Serial.begin(9600);
  pinMode(inputPin, INPUT);  // declare sensor as input
  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(server);
  mqttClient.enableLastWillMessage("lwt", "I am going offline");
  mqttClient.setKeepAlive(30);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("connecting, pls wait");
    delay(1000);
  }
  Serial.println("you're connected!");
  WiFi.setHostname("c3test");
  mqttClient.loopStart();
}

void loop() {
  val = digitalRead(inputPin);  // read input value
  if (val == HIGH)              // check if the input is HIGH
  {

    if (pirState == LOW) {
      Serial.println("Motion detected!");  // print on output change
      mqttClient.publish(publishTopic, String(val), 0, false);
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH) {
      Serial.println("Motion ended!");  // print on output change
      mqttClient.publish(publishTopic, String(val), 0, false);
      pirState = LOW;
    }
  }
}

void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client) {
  if (mqttClient.isMyTurn(client))  // can be omitted if only one client
  {
    mqttClient.subscribe(subscribeTopic,
                         [](const String &payload) {
                           Serial.println("From: " + String(subscribeTopic) + String(" Message: ") + String(payload.c_str()));
                         });

    mqttClient.subscribe("bar/#", [](const String &topic, const String &payload) {
      Serial.println(String(subscribeTopic) + String(" ") + String(payload.c_str()));
    });
  }
}

esp_err_t handleMQTT(esp_mqtt_event_handle_t event) {
  mqttClient.onEventCallback(event);
  return ESP_OK;
}