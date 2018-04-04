// slackbot testing using https://github.com/urish/arduino-slack-bot
/**
   Arduino Real-Time Slack Bot
   Copyright (C) 2016, Uri Shaked.
   Licensed under the MIT License
*/

/*
  modified by chaeplin @ gmail.com
*/

#include <time.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>

extern "C" {
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/slack_2hrs_setting.h"

// slack.com : Expires: Saturday, 2 February 2019 at 08:59:59 Korean Standard Time
const char* api_fingerprint = "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3";

#define BUTTON_PIN 12
#define BUZZER_PIN 14
#define LED_BLUE 2
#define LED_YELL 0

WiFiClientSecure sslclient;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool connected = false;
bool bsendalarm = false;
volatile int remindMinutes = 120;

volatile unsigned long startMills = 0;
volatile unsigned long ledMills = 0;
volatile unsigned long remainMills = 0;

volatile bool buzzer_ring = false;

volatile bool bbutton_isr;
bool button_status;
volatile uint32_t button_interuptCount = 0;
uint32_t prev_interuptCount = 0;

unsigned long lastPing = 0;

Ticker ticker, ticker_buzzer;

void tick()
{
  //toggle state
  int state = digitalRead(LED_YELL);
  digitalWrite(LED_YELL, !state);
}

void tick_buzzer()
{
  //toggle state
  int state_buzzer = digitalRead(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, !state_buzzer);
}

void ICACHE_RAM_ATTR button_isr() {
  if (bbutton_isr == false) {
    button_interuptCount++;
    bbutton_isr = true;
  }
}

void send2hrs()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg = "chaeplin: check things";

  root["text"] = msg.c_str();
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendAck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg = "Ack button pressed";

  root["text"] = msg.c_str();
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendCheck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  remainMills = ((remindMinutes * 60 * 1000) -  (millis() - startMills)) / (60 * 1000);

  String msg = "alarm ";

  if (bsendalarm) {
    msg += "will fire after ";
    msg += remainMills;
    msg += " mins";
  } else {
    msg += "not set, setting is ";
    msg += remindMinutes;
    msg += " mins";
  }

  root["text"] = msg.c_str();
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

/**
  Sends a ping message to Slack. Call this function immediately after establishing
  the WebSocket connection, and then every 5 seconds to keep the connection alive.
*/
void sendPing()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendHello()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;
  root["text"] = "Hello world 2hrsbot";
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}


void ICACHE_RAM_ATTR processSlackMessage(String receivedpayload)
{
  //char json[] = "{\"type\":\"message\",\"channel\":\"XXXXXXXX\",\"user\":\"XXXXXXXX\",\"text\":\"xxxxxxxxxxxx\",\"ts\":\"1491047008.621282\",\"source_team\":\"XXXXXXXX\",\"team\":\"XXXXXXXX\"}";
  char json[] = "{\"type\":\"message\",\"channel\":\"XXXXXXXX\",\"user\":\"XXXXXXXX\",\"client_msg_id\":\"BBA0F81E-8DF6-46C9-8DB7-F0ACF11E1C5E\",\"event_ts\":\"1522027419.000121\"\"text\":\"xxxxxxxxxxxx\",\"ts\":\"1491047008.621282\",\"source_team\":\"XXXXXXXX\",\"team\":\"XXXXXXXX\"}";
  receivedpayload.toCharArray(json, 1204);
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    Serial.println("root failed");
    return;
  }
  if (root.containsKey("text"))
  {
    const char* text = root["text"];
    Serial.printf("[Processing] text: %s\n", text);

    if (String(text) == "check") {
      sendCheck();
    } else if (String(text) == "on") {
      Serial.println("->on");

      bsendalarm = true;
      startMills = millis();
      ledMills = millis();
      ticker.attach(1, tick);
      sendCheck();

    } else if (String(text) == "off") {
      Serial.println("->off");
      bsendalarm = false;
      ticker.detach();
      digitalWrite(LED_YELL, LOW);

      buzzer_ring = false;
      ticker_buzzer.detach();
      digitalWrite(BUZZER_PIN, HIGH);
      sendCheck();
    } else {
      int num = atoi(text);
      if (num >= 1 && num <= 1440) {
        Serial.print("->timer : ");
        Serial.println(num);
        remindMinutes = num;
      }
      sendCheck();
    }
  }
}

/**
  Called on each web socket event. Handles disconnection, and also
  incoming messages from slack.
*/
void webSocketEvent(WStype_t type, uint8_t *payload, size_t len)
{
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      sendPing();
      webSocket.loop();
      sendHello();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);

      String receivedpayload;
      for (int i = 0; i < len; i++)
      {
        receivedpayload += (char)payload[i];
      }
      if (receivedpayload.startsWith("{\"type\":\"message"))
      {
        if (receivedpayload.indexOf(SLACK_CHANNEL) != -1 &&
            receivedpayload.indexOf(SLACK_USER) != -1 &&
            receivedpayload.indexOf(SLACK_TEAM) != -1 )
        {
          processSlackMessage(receivedpayload);
        }
      }
      break;
  }
}

/**
  Establishes a bot connection to Slack:
  1. Performs a REST call to get the WebSocket URL
  2. Conencts the WebSocket
  Returns true if the connection was established successfully.
*/
bool connectToSlack()
{
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  String uri_to_post = "/api/rtm.start?token=";
  uri_to_post += SLACK_BOT_TOKEN;

  http.begin("slack.com", 443, uri_to_post, api_fingerprint);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  // Step 2: Open WebSocket connection and register event handler
  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);
  return true;
}

void wifi_connect() {
  Serial.print("[WIFI] start millis     : ");
  Serial.println(millis());
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
  Serial.print("[WIFI] connected millis : ");
  Serial.print(millis());
  Serial.print(" - ");
  Serial.println(WiFi.localIP());
}

void get_ntptime() {
  Serial.println("Setting time using SNTP");
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 1000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    Serial.println(now);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  bbutton_isr = false;
  attachInterrupt(BUTTON_PIN, button_isr, CHANGE);

  pinMode(LED_BLUE, OUTPUT);
  pinMode(LED_YELL, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, HIGH);

  wifi_connect();
  get_ntptime();

}

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    webSocket.loop();

    if (bbutton_isr) {
      button_status = digitalRead(BUTTON_PIN);
      Serial.print("[button_status] ---> bbutton_isr detected : ");
      Serial.print(button_status);
      Serial.print(" button_interuptCount : ");
      Serial.println(button_interuptCount);

      if (button_status) {
        if (buzzer_ring and !bsendalarm) {
          Serial.println("->Alarm off by button");
          bsendalarm = false;
          buzzer_ring = false;

          ticker_buzzer.detach();
          ticker.detach();
          digitalWrite(LED_YELL, LOW);
          digitalWrite(BUZZER_PIN, HIGH);

          sendAck();
          sendCheck();          
        } else if (!bsendalarm) {
          Serial.println("->Alarm set by button");
          bsendalarm = true;
          startMills = millis();
          ledMills = millis();
          ticker.attach(1, tick);
          sendCheck();
        }
      }

        bbutton_isr = false;
      }

      if (connected) {
        digitalWrite(LED_BLUE, HIGH);
        // Send ping every 5 seconds, to keep the connection alive
        if (millis() - lastPing > 5000)
        {
          sendPing();
          lastPing = millis();
        }

        if (bsendalarm) {
          if ((millis() - startMills) > (remindMinutes * 60 * 1000)) {
            send2hrs();
            ticker_buzzer.attach(1, tick_buzzer);
            buzzer_ring = true;
            bsendalarm = false;
            ticker.detach();
            digitalWrite(LED_YELL, LOW);
          }
        }
      } else {
        // Try to connect / reconnect to slack
        connected = connectToSlack();
        if (!connected)
        {
          digitalWrite(LED_BLUE, LOW);
          delay(500);
        }
      }
    } else {
      wifi_connect();
      get_ntptime();
    }
  }
