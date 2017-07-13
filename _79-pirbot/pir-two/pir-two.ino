// slackbot testing using https://github.com/urish/arduino-slack-bot
/**
   Arduino Real-Time Slack Bot
   Copyright (C) 2016, Uri Shaked.
   Licensed under the MIT License
*/

/*
 modified by chaeplin @ gmail.com
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

extern "C" {
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/slack_pir2_setting.h"

// slack.com : Expires: Saturday, 2 February 2019 at 08:59:59 Korean Standard Time
const char* api_fingerprint = "AC 95 5A 58 B8 4E 0B CD B3 97 D2 88 68 F5 CA C1 0A 81 E3 6E";

#define DOOR_PIN 12


//#define SLACK_BOT_TOKEN "put-your-slack-token-here"
//#define SLACK_CHANNEL "xxxxxx"
//#define SLACK_USER "xxxxxxx"
//#define SLACK_TEAM "xxxxxx"

//#define WIFI_SSID       "wifi-name"
//#define WIFI_PASSWORD   "wifi-password"

WiFiClientSecure sslclient;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool connected = false;

volatile bool bdoor_isr;
bool door_status;
volatile uint32_t door_interuptCount = 0;
uint32_t prev_interuptCount = 0;

volatile bool bsendstatus = false;

void ICACHE_RAM_ATTR door_isr() {
  if (bdoor_isr == false) {
    door_interuptCount++;
    bdoor_isr = true;
  }
}


void sendCheck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;
  
  String msg = "pir-two : detected ";
  
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
  root["text"] = "Hello world - pir-two";
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}


void ICACHE_RAM_ATTR processSlackMessage(String receivedpayload) 
{
  char json[] = "{\"type\":\"message\",\"channel\":\"XXXXXXXX\",\"user\":\"XXXXXXXX\",\"text\":\"xxxxxxxxxxxx\",\"ts\":\"1491047008.621282\",\"source_team\":\"XXXXXXXX\",\"team\":\"XXXXXXXX\"}";
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

    if (String(text) == "piron")
    {
      bsendstatus = true;
      sendCheck();
    }

    if (String(text) == "piroff")
    {
      bsendstatus = false;
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

void setup() {
  Serial.begin(115200);
  //Serial.setDebugOutput(true);


  pinMode(DOOR_PIN, INPUT);
  bdoor_isr = false;
  bsendstatus = false;
  attachInterrupt(DOOR_PIN, door_isr, CHANGE);


  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(100);
  }

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

unsigned long lastPing = 0;

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void loop() 
{
  webSocket.loop();

  if (bdoor_isr && bsendstatus) {
    door_status = digitalRead(DOOR_PIN);
    Serial.print("[door_status] ---> bdoor_isr detected : ");
    Serial.print(door_status);
    Serial.print(" door_interuptCount : ");
    Serial.println(door_interuptCount);
    sendCheck();
    bdoor_isr = false;
  }

  if (connected) {
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
      delay(500);
    }
  }
}
