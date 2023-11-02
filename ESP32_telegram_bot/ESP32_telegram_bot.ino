#include "Arduino.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "ESP32MQTTClient.h"
#include <UniversalTelegramBot.h>   // Universal Telegram Bot Library written by Brian Lough: https://github.com/witnessmenow/Universal-Arduino-Telegram-Bot
#include <ArduinoJson.h>
#include <ESP32Servo.h>

//MQTT set up
const char* ssid = "notyouriphone";
const char* password = "hidejy123";
char *server = "mqtt://172.20.10.3:1883"; 
char *subscribeTopic = "arduino/command";
ESP32MQTTClient mqttClient; 
int passcount = 0;

//mqttClient.publish("other parameters", String(event.temperature), 0, false);

//Servo parameters
int APin = 13;
ESP32PWM pwm;
int freq = 50;

//Timerwakeup setup
#define uS_TO_S_FACTOR 1000000ULL 
#define TIME_TO_SLEEP  180    
RTC_DATA_ATTR int bootCount = 0;

// Initialize Telegram BOT
#define BOTtoken "6635330110:AAHd34EJKJ4BG_y0_jAt4C5gkE6qSH44j2I"  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
#define CHAT_ID "1775195171"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Checks for new messages every 1 second.
int botRequestDelay = 50;
unsigned long lastTimeBotRan;
// To store received Telegram message
String telegramMessage = ""; 

// Handle what happens when you receive new messages
void handleNewMessages(int numNewMessages) {
  Serial.println("handleNewMessages");
  Serial.println(String(numNewMessages));

  for (int i=0; i<numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Unauthorized user", "");
      continue;
    }
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);

    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
    String welcome = "Welcome, " + from_name + ".\n";
    welcome += "Use the following commands to control your outputs.\n\n";
    welcome += "/unlock to unlock the door \n";
    welcome += "/lock to lock the door \n";
    bot.sendMessage(chat_id, welcome, "");
    }

    if (text == "/unlock") {
    bot.sendMessage(chat_id, "Unlocking door", "");
    // pwm.writeScaled(0.025);
    }

    if (text == "/lock") {
    bot.sendMessage(chat_id, "Locking door", "");
    // pwm.writeScaled(0.075);
    }
    if (text == "/unlock" || text == "/lock") {
      telegramMessage = text;
      mqttClient.publish("telegram/command", text.c_str());
    }
  }
}

void setup() {
  Serial.begin(115200);
  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  mqttClient.enableDebuggingMessages();
  mqttClient.setURI(server);
  mqttClient.enableLastWillMessage("lwt", "I am going offline");
  mqttClient.setKeepAlive(30);
  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) 
      {
          Serial.print('.');
          delay(1000);
      }
  Serial.println("Connected to wifi");
  WiFi.setHostname("c3test");
  mqttClient.loopStart();
    //Servo setup
  ESP32PWM::allocateTimer(0);
  pwm.attachPin(APin, freq, 10);
  Serial.println("Connected to wifi");
  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());
}

void loop() {
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while(numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}

void handleClassificationResponse(const String &payload) {
  if (bot.messages[0].chat_id) {
    String chat_id = String(bot.messages[0].chat_id);
  if(payload == "password correct" && passcount <=3) {
    bot.sendMessage(chat_id, "People entered correct password, door opening", "");
    pwm.writeScaled(0.025);
    passcount = 0;
  } else if(payload == "wrong password" && passcount <=3) {
    bot.sendMessage(chat_id, "People entered incorrect password, door remained locked", "");
    passcount++;
  } else if(payload == "wrong password" && passcount >= 4) {
    bot.sendMessage(chat_id, "too many attempt to open the door,door will remain locked for 3 min", "");
    passcount++; 
    sleepDevice();
    }
  }
    if (payload == "unlock") {
    pwm.writeScaled(0.025);
  } else if (payload == "lock") {
    pwm.writeScaled(0.075);
  }
}

void sleepDevice() {
  Serial.println("Going to sleep for " + String(TIME_TO_SLEEP) + " seconds...");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}

void onConnectionEstablishedCallback(esp_mqtt_client_handle_t client)
{
    if (mqttClient.isMyTurn(client)) // can be omitted if only one client
    {    
        mqttClient.subscribe(subscribeTopic, [](const String &payload)
                             {Serial.println(String(subscribeTopic) + String(" ") + String(payload.c_str())) ; 
                              handleClassificationResponse(payload);
                             });

        mqttClient.subscribe("telegram/command", [](const String &payload)
                             {Serial.println(String("telegram/command ") + String(payload.c_str())); 
                              handleTelegramCommand(payload);
                             });

        mqttClient.subscribe("bar/#", [](const String &topic, const String &payload)
                             {Serial.println(String(subscribeTopic)+String(" ")+String(payload.c_str())); });
    }
}

void handleTelegramCommand(const String &payload) {
  if (payload == "/unlock" || payload == "/lock") {
    handleClassificationResponse(payload);
  }
}

esp_err_t handleMQTT(esp_mqtt_event_handle_t event)
  {
    mqttClient.onEventCallback(event);
    return ESP_OK;
  }