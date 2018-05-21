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

extern "C" {
#include "user_interface.h"
}

#include "/usr/local/src/apha_setting.h"
#include "/usr/local/src/slack_door_setting.h"

const char* api_fingerprint = "C1 0D 53 49 D2 3E E5 2B A2 61 D5 9E 6F 99 0D 3D FD 8B B2 B3";

#define DOOR_PIN 14
#define PIR_PIN 12
#define LEDOUT_PIN 2

WiFiClientSecure sslclient;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool connected = false;
unsigned long lastPing = 0;

bool door_status;
volatile bool bdoor_isr;
volatile uint32_t door_interuptCount = 0;

bool pir_status;
volatile bool bpir_isr;
volatile uint32_t pir_interuptCount = 0;

void ICACHE_RAM_ATTR door_isr() 
{
  if (bdoor_isr == false) {
    door_interuptCount++;
    bdoor_isr = true;
  }
}

void ICACHE_RAM_ATTR pir_isr() 
{
  if (bpir_isr == false) {
    pir_interuptCount++;
    bpir_isr = true;
  }
}


void send_door_Check()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg = "door status : ";
  if (door_status) {
    msg += "open";
  } else {
    msg += "closed";
  }

  root["text"] = msg.c_str();
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}


void send_pir_check()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg = "pir status : ";
  if (pir_status) {
    msg += "detected";
  } else {
    msg += "not detected";
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
  root["text"] = "Hello world";
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}


void ICACHE_RAM_ATTR processSlackMessage(String receivedpayload)
{
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

    if (String(text) == "check")
    {
      send_door_Check();
      send_pir_check();
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

void get_ntptime() 
{
  Serial.println("Setting time using SNTP");
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  while (now < 5000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
    Serial.println(now);
  }
  delay(500);
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void wifi_connect() 
{
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

void setup() 
{
  Serial.begin(115200);
  //Serial.setDebugOutput(true);

  pinMode(PIR_PIN, INPUT);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  pinMode(LEDOUT_PIN, OUTPUT);

  bdoor_isr = false;
  attachInterrupt(DOOR_PIN, door_isr, CHANGE);
  attachInterrupt(PIR_PIN, pir_isr, RISING);

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

    if (bdoor_isr) {
      door_status = digitalRead(DOOR_PIN);
      Serial.print("[door_status] ---> bdoor_isr detected : ");
      Serial.print(door_status);
      Serial.print(" door_interuptCount : ");
      Serial.println(door_interuptCount);
      send_door_Check();
      bdoor_isr = false;
    }

    if (bpir_isr) {
      pir_status = digitalRead(PIR_PIN);
      Serial.print("[pir_status] ---> bpir_isr detected : ");
      Serial.print(pir_status);
      Serial.print(" pir_interuptCount : ");
      Serial.println(pir_interuptCount);
      send_pir_check();
      bpir_isr = false;
    }

    if (connected) {
      digitalWrite(LEDOUT_PIN, HIGH);
      // Send ping every 5 seconds, to keep the connection alive
      if (millis() - lastPing > 5000)
      {
        sendPing();
        lastPing = millis();
      }
    }
    else
    {
      // Try to connect / reconnect to slack
      connected = connectToSlack();
      if (!connected)
      {
        digitalWrite(LEDOUT_PIN, LOW);
        delay(500);
      }
    }
  } else {
    wifi_connect();
    get_ntptime();
  }
}
