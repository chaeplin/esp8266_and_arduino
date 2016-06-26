// 80MHZ NODEMCU V.1
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
// https://github.com/Links2004/arduinoWebSockets
#include <WebSocketsClient.h>
// https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
// https://github.com/chaeplin/lgwhisen
#include <lgWhisen.h>
// https://github.com/markszabo/IRremoteESP8266
#include <IRremoteESP8266.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/slack_setting.h"
const char* otapassword = OTA_PASSWORD;

const char* api_fingerprint = "AB F0 5B A9 1A E0 AE 5F CE 32 2E 7C 66 67 49 EC DD 6D 6A 38";

//#define SLACK_BOT_TOKEN "put-your-slack-token-here"
//#define SLACK_CHANNEL "xxxxxx"
//#define SLACK_USER "xxxxxxx"
//#define SLACK_TEAM "xxxxxx"

//#define WIFI_SSID       "wifi-name"
//#define WIFI_PASSWORD   "wifi-password"

WiFiClientSecure sslclient;
WebSocketsClient webSocket;
WiFiUDP udp;

long nextCmdId = 1;
bool connected = false;

unsigned long nextTimercheck;

// AC
#define IR_RX_PIN 14
#define IR_TX_PIN 2
#define AC_CONF_TYPE 1
#define AC_CONF_HEATING 0
#define AC_CONF_ON_MIN 5
#define AC_CONF_OFF_MIN 3

volatile struct
{
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
  bool haveData;
  bool timermode;
  bool timerfirst;
  unsigned long intervalon;  // ms
  unsigned long intervaloff; // ms
  unsigned long timerMillis;
} ir_data;

IRrecv irrecv(IR_RX_PIN);
lgWhisen lgWhisen(AC_CONF_TYPE, AC_CONF_HEATING);

void chane_ac_temp_flow()
{
  if (ir_data.ac_mode == 1)
  {
    lgWhisen.setTemp(ir_data.ac_temp);
    lgWhisen.setFlow(ir_data.ac_flow);
    lgWhisen.activate();
  }
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
  //root["text"] = "Hello world";
  root["text"] = "테스트 시작";
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

  String msg = "ac status: ";
  if (ir_data.timermode)
  {
    int timeremain = ((nextTimercheck - (millis() - ir_data.timerMillis)) / 1000 ) / 60;
    if (ir_data.ac_mode == 0)
    {
      msg += "timer:off, next change in ";
      msg += timeremain;
      msg += " min\n";
    }
    else
    {
      msg += "timer:on, next change in ";
      msg += timeremain;
      msg += " min\n";
    }
  }
  else
  {
    if (ir_data.ac_mode == 0)
    {
      msg += "off\n";
    }
    else
    {
      msg += "on\n";
    }
  }
  msg += "ac temp set : ";
  msg += ir_data.ac_temp;
  msg += "\n";
  msg += "ac flow set : ";
  msg += ir_data.ac_flow;
  msg += " \r\n\r\n";

  root["text"] = msg.c_str();
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendHelp()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg =  "on -> ac on\n";
  msg += "off -> ac off\n";
  msg += "ton -> timer start with ac on\n";
  msg += "[18 ~ 30] -> temperature set\n";
  if (AC_CONF_TYPE == 0)
  {
    msg += "[0 ~ 2] -> flow set(low/mid/high)\n";
  }
  else
  {
    msg += "[0 ~ 3] -> flow set(low/mid/high/change)\n";
  }
  msg += "check -> report ac status\r\n";

  root["text"] = msg.c_str();
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void ICACHE_RAM_ATTR processSlackMessage(String receivedpayload)
{
  char json[] = "{\"type\":\"message\",\"user\":\"XX0990XX\",\"text\":\"XX0990XX\",\"team\":\"XX0990XX\",\"user_team\":\"XX0990XX\",\"user_profile\":{\"avatar_hash\":\"g9c0531af70a\",\"image_72\":\"https://secure.gravatar.com/avatar/9c0531af70ad0dbed618e9f4c2198145.jpg?s=72&d=https%3A%2F%2Fa.slack-edge.com%2F66f9%2Fimg%2Favatars%2Fava_0019-72.png\",\"first_name\":null,\"real_name\":\"\",\"name\":\"XX0990XX\"},\"channel\":\"XX0990XX\",\"ts\":\"1466954463.000275\"}";
  receivedpayload.toCharArray(json, 1024);
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    Serial.println("root failed");
    return;
  }

  if (root.containsKey("text"))
  {
    const char* text = root["text"].asString();
    Serial.printf("[Processing] text: %s\n", text);
    if (String(text) == "help")
    {
      sendHelp();
    }
    else if (String(text) == "check")
    {
      Serial.println("->check");
      sendCheck();
    }
    else if (String(text) == "on")
    {
      Serial.println("->on");
      ir_data.ac_mode   = 1;
      ir_data.timermode = false;
      ir_data.haveData  = true;
    }
    else if (String(text) == "off")
    {
      Serial.println("->off");
      ir_data.ac_mode   = 0;
      ir_data.timermode = false;
      ir_data.haveData  = true;
    }
    else if (String(text) == "ton")
    {
      Serial.println("->ton");
      ir_data.ac_mode     = 0;
      ir_data.timermode   = true;
      ir_data.timerfirst  = false;
    }
    else if (String(text) == "0")
    {
      Serial.println("->flow 0");
      ir_data.ac_flow = 0;
      chane_ac_temp_flow();
      sendCheck();
    }
    else
    {
      uint8_t num = atoi(text);
      if (num >= 18 && num <= 30)
      {
        Serial.print("->temp : ");
        Serial.println(num);
        ir_data.ac_temp = num;
        chane_ac_temp_flow();
        sendCheck();
      }
      else if (num >= 1 && num <= 3)
      {
        if (AC_CONF_TYPE == 0 && num == 3)
        {
          return;
        }
        else
        {
          Serial.print("->flow : ");
          Serial.println(num);
          ir_data.ac_flow = num;
          chane_ac_temp_flow();
          sendCheck();
        }
      }
    }

  }
}

/**
  Called on each web socket event. Handles disconnection, and also
  incoming messages from slack.
*/
void ICACHE_RAM_ATTR webSocketEvent(WStype_t type, uint8_t *payload, size_t len)
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
          if (len < 1024) {
            processSlackMessage(receivedpayload);
          }
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
  Serial.println();
  Serial.println("Starting....... ");
  //Serial.setDebugOutput(true);

  // ac
  ir_data.ac_mode     = 0;
  ir_data.ac_temp     = 27;
  ir_data.ac_flow     = 1;
  ir_data.haveData    = false;
  ir_data.timermode   = false;
  ir_data.timerfirst  = false;
  ir_data.intervalon  = (AC_CONF_ON_MIN * 60 * 1000); // ms
  ir_data.intervaloff = (AC_CONF_OFF_MIN * 60 * 1000); // min
  ir_data.timerMillis = millis();;
  nextTimercheck = ir_data.intervalon;

  lgWhisen.setTemp(ir_data.ac_temp);    // 18 ~ 30
  lgWhisen.setFlow(ir_data.ac_flow);    // 0 : low, 1 : mid, 2 : high, if setActype == 1, 3 : change
  lgWhisen.setIrpin(IR_TX_PIN);         // ir tx pin

  irrecv.enableIRIn();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(". ");
    delay(100);
  }
  Serial.println();

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-slackbot");
  ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]()
  {
    //sendUdpSyslog("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]()
  {
    //sendUdpSyslog("ArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    //syslogPayload = "Progress: ";
    //syslogPayload += (progress / (total / 100));
    //sendUdpSyslog(syslogPayload);
  });
  ArduinoOTA.onError([](ota_error_t error)
  {
    //ESP.restart();
    if (error == OTA_AUTH_ERROR) abort();
    else if (error == OTA_BEGIN_ERROR) abort();
    else if (error == OTA_CONNECT_ERROR) abort();
    else if (error == OTA_RECEIVE_ERROR) abort();
    else if (error == OTA_END_ERROR) abort();
  });

  ArduinoOTA.begin();
}

unsigned long lastPing = 0;

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void loop()
{
  // ac ir rx
  decode_results results;

  if (irrecv.decode(&results))
  {
    if (lgWhisen.decode(&results))
    {
      if (lgWhisen.get_ir_mode() != 0)
      {
        ir_data.ac_mode = 1;
        ir_data.timermode = false;
      }
      else
      {
        ir_data.ac_mode = 0;
        ir_data.timermode = false;
      }
      if (lgWhisen.get_ir_temperature() != 255)
      {
        ir_data.ac_temp = lgWhisen.get_ir_temperature();
      }
      if (lgWhisen.get_ir_flow() != 255)
      {
        ir_data.ac_flow = lgWhisen.get_ir_flow();
      }
      sendCheck();
    }
    irrecv.enableIRIn();
  }

  // slack
  webSocket.loop();

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

  // ac change, ir tx
  if (ir_data.haveData)
  {
    lgWhisen.setTemp(ir_data.ac_temp);
    lgWhisen.setFlow(ir_data.ac_flow);

    switch (ir_data.ac_mode)
    {
      // ac power down
      case 0:
        Serial.println("IR -----> AC Power Down");
        irrecv.disableIRIn();
        lgWhisen.power_down();
        delay(5);
        irrecv.enableIRIn();
        break;

      // ac on
      case 1:
        Serial.println("IR -----> AC Power On");
        irrecv.disableIRIn();
        lgWhisen.activate();
        delay(5);
        irrecv.enableIRIn();
        break;

      default:
        break;
    }
    sendCheck();
    ir_data.haveData = false;
  }

  // ac timer
  if (ir_data.timermode)
  {
    if (((millis() - ir_data.timerMillis) > nextTimercheck) || !ir_data.timerfirst)
    {
      if (ir_data.ac_mode == 0)
      {
        ir_data.ac_mode = 1;
        nextTimercheck = ir_data.intervalon;
      }
      else
      {
        ir_data.ac_mode = 0;
        nextTimercheck = ir_data.intervaloff;
      }
      ir_data.haveData = true;
      ir_data.timerfirst  = true;
      ir_data.timerMillis = millis();
    }
  }
  ArduinoOTA.handle();
}

// end
