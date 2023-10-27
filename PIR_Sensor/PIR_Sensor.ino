#include <WiFi.h>
#include "ESP32MQTTClient.h"

const char *ssid = "notyouriphone";
const char *pass = "hidejy123";
char *server = "mqtt://172.20.10.2:1883";
char *subscribeTopic = "power"; // maybe subscribe to other sensor to turn on this as well
ESP32MQTTClient mqttClient;

int inputPin = 8;               // choose the input pin (for PIR sensor)
int pirState = LOW;             // we start, assuming no motion detected
int val = 0;                    // variable for reading the pin status
 
void setup() {
  //mqtt setup
   Serial.begin(115200);
  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(server);
  mqttClient.enableLastWillMessage("lwt", "I am going offline");
  mqttClient.setKeepAlive(30);
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) 
  {
      Serial.print('.');
      delay(1000);
  }
  Serial.println("Connected to wifi");
  WiFi.setHostname("c3test");
  mqttClient.loopStart();
  //PIR input setup
  pinMode(inputPin, INPUT);     // declare sensor as input
}
 
void loop() {
  val = digitalRead(inputPin);  // read input value

  if (val == HIGH) {            // check if the input is HIGH
    if (pirState == LOW) {
      Serial.println("Motion detected!");  // print on output change
      mqttClient.publish("motion", "object detected");
      pirState = HIGH;
    }
  } else {
    if (pirState == HIGH) {
      Serial.println("Motion ended!");  // print on output change
      mqttClient.publish("motion", "no object");
      pirState = LOW;
    }
  }
}

void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client)
{
    if (mqttClient.isMyTurn(client)) // can be omitted if only one client
    {    
        mqttClient.subscribe(subscribeTopic, [](const String &payload)
                          {Serial.println(String(subscribeTopic)+String(" ")+String(payload.c_str())); });

        mqttClient.subscribe("bar/#", [](const String &topic, const String &payload)
                             {Serial.println(String(subscribeTopic)+String(" ")+String(payload.c_str())); });
    }
}

esp_err_t handleMQTT(esp_mqtt_event_handle_t event)
  {
    mqttClient.onEventCallback(event);
    return ESP_OK;
  }

