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

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/slack_setting.h"

const char* api_fingerprint = "AB F0 5B A9 1A E0 AE 5F CE 32 2E 7C 66 67 49 EC DD 6D 6A 38";

//#define SLACK_BOT_TOKEN "put-your-slack-token-here"
//#define WIFI_SSID       "wifi-name"
//#define WIFI_PASSWORD   "wifi-password"

WiFiClientSecure sslclient;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool connected = false;

/**
  Sends a ping message to Slack. Call this function immediately after establishing
  the WebSocket connection, and then every 5 seconds to keep the connection alive.
*/
void sendPing() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void processSlackMessage(char *payload) {
// [WebSocket] Message: {"type":"message","channel":"xxxxx","user":"xxxx","text":"test123","ts":"1466878878.000005","team":"xxxx"}  
}

/**
  Called on each web socket event. Handles disconnection, and also
  incoming messages from slack.
*/
void webSocketEvent(WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      sendPing();
      break;

    case WStype_TEXT:
      Serial.printf("[WebSocket] Message: %s\n", payload);
      processSlackMessage((char*)payload);
      break;
  }
}

/**
  Establishes a bot connection to Slack:
  1. Performs a REST call to get the WebSocket URL
  2. Conencts the WebSocket
  Returns true if the connection was established successfully.
*/
bool connectToSlack() {
  // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
  HTTPClient http;
  String uri_to_post = "/api/rtm.start?token=";
  uri_to_post += SLACK_BOT_TOKEN;

  http.begin("slack.com", 443, uri_to_post, api_fingerprint);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
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

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

unsigned long lastPing = 0;

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void loop() {
  webSocket.loop();

  if (connected) {
    // Send ping every 5 seconds, to keep the connection alive
    if (millis() - lastPing > 5000) {
      sendPing();
      lastPing = millis();
    }
  } else {
    // Try to connect / reconnect to slack
    connected = connectToSlack();
    if (!connected) {
      delay(500);
    }
  }
}
