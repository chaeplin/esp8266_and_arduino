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
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
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
#include <Average.h>

extern "C" {
#include "user_interface.h"
}
#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/slack_setting.h"

const char* api_fingerprint = "AB F0 5B A9 1A E0 AE 5F CE 32 2E 7C 66 67 49 EC DD 6D 6A 38";

//#define SLACK_BOT_TOKEN "put-your-slack-token-here"
//#define SLACK_CHANNEL "xxxxxx"
//#define SLACK_USER "xxxxxxx"
//#define SLACK_TEAM "xxxxxx"

//#define WIFI_SSID       "wifi-name"
//#define WIFI_PASSWORD   "wifi-password"
//#definr OTA_PASSWORD    "ota-password"

//
IPAddress mqtt_server = MQTT_SERVER;

WiFiClient wifiClient;
WebSocketsClient webSocket;
WiFiUDP udp;

long nextCmdId = 1;
bool connected = false;

// AC
#define IR_RX_PIN 14
#define IR_TX_PIN 2
#define AC_CONF_TYPE 1
#define AC_CONF_HEATING 0
#define AC_CONF_ON_MIN 30
#define AC_CONF_OFF_MIN 20

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
  unsigned long nextTimercheck;
} ir_data;

// mqtt
void callback(char* intopic, byte* inpayload, unsigned int length);

const char* hellotopic  = "HELLO";
const char* willTopic   = "clients/nodemcu";
const char* willMessage = "0";
const char* substopic   = "esp8266/arduino/solar";

String clientName;

PubSubClient mqttclient(mqtt_server, 1883, callback, wifiClient);

// Timelib
unsigned int localPort = 12390;
const int timeZone = 9;

// temp received by mqtt
Average<float> ave(20);

// ir
IRrecv irrecv(IR_RX_PIN);
lgWhisen lgWhisen;

boolean reconnect() {
  if (!mqttclient.connected())
  {
    if (mqttclient.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage))
    {
      mqttclient.publish(willTopic, "1", true);
      mqttclient.loop();
      mqttclient.publish(hellotopic, "hello again 1 from nodemcu ");
      mqttclient.loop();
      mqttclient.subscribe(substopic);
      mqttclient.loop();
      yield();
    }
  }
  return mqttclient.connected();
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  Serial.printf("[MQTT] payload: %s\n", receivedpayload.c_str());
  if (length <= 200)
  {
    parseMqttMsg(receivedpayload, receivedtopic);
  }
}

void parseMqttMsg(String receivedpayload, String receivedtopic)
{
  char json[] = "{\"DS18B20\":28.00,\"FreeHeap\":32384,\"RSSI\":-74,\"millis\":2413775}";

  receivedpayload.toCharArray(json, 200);
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    return;
  }

  if (root.containsKey("DS18B20"))
  {
    ave.push(root["DS18B20"]);
  }
}

void chane_ac_temp_flow()
{
  if (ir_data.ac_mode == 1)
  {
    lgWhisen.setTemp(ir_data.ac_temp);
    lgWhisen.setFlow(ir_data.ac_flow);
    irrecv.disableIRIn();
    lgWhisen.activate();
    delay(5);
    irrecv.enableIRIn();
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

  String msg = "ac status : ";
  if (ir_data.timermode)
  {
    int timeremain = ((ir_data.nextTimercheck - (millis() - ir_data.timerMillis)) / 1000 ) / 60;
    if (ir_data.ac_mode == 0)
    {
      msg += ":timer_clock::black_square_for_stop:, next change in ";
      msg += timeremain;
      msg += " min";
    }
    else
    {
      msg += ":timer_clock::arrows_counterclockwise:, next change in ";
      msg += timeremain;
      msg += " min";
    }
  }
  else
  {
    if (ir_data.ac_mode == 0)
    {
      msg += ":black_square_for_stop:";
    }
    else
    {
      msg += ":arrows_counterclockwise:";
    }
  }

  msg += "\nac :thermometer: set : ";
  msg += ir_data.ac_temp;
  msg += "\nac flow set : ";
  msg += ir_data.ac_flow;
  msg += "\ncurr :thermometer: : ";
  msg += ave.mean();

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
  msg += "check -> report ac status";

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
      return;
    }
    else if (String(text) == "check")
    {
      Serial.println("->check");
      sendCheck();
      return;
    }
    else if (String(text) == "on")
    {
      Serial.println("->on");
      ir_data.ac_mode   = 1;
      ir_data.timermode = false;
      ir_data.haveData  = true;
      return;
    }
    else if (String(text) == "off")
    {
      Serial.println("->off");
      ir_data.ac_mode   = 0;
      ir_data.timermode = false;
      ir_data.haveData  = true;
      return;
    }
    else if (String(text) == "ton")
    {
      Serial.println("->ton");
      ir_data.ac_mode     = 0;
      ir_data.timermode   = true;
      ir_data.timerfirst  = false;
      return;
    }
    else if (String(text) == "0")
    {
      Serial.println("->flow 0");
      ir_data.ac_flow = 0;
      chane_ac_temp_flow();
      sendCheck();
      return;
    }
    else
    {
      if (String(text).length() == 2)
      {
        uint8_t num = atoi(text);
        if (num >= 18 && num <= 30)
        {
          Serial.print("->temp : ");
          Serial.println(num);
          ir_data.ac_temp = num;
          chane_ac_temp_flow();
          sendCheck();
          return;
        }
      }

      if (String(text).length() == 1)
      { 
        uint8_t num = atoi(text);   
        if (num >= 1 && num <= 3)
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
            return;
          }
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
          if (len < 1024)
          {
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
{ // Step 1: Find WebSocket address via RTM API (https://api.slack.com/methods/rtm.start)
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

void wifi_connect()
{
  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.hostname("esp-slackbot");

  int Attempt = 1;
  Serial.println("[WIFI] connecting...");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(". ");
    if ( Attempt % 30 == 0 ) {
      Serial.println();
    }
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      Serial.println();
      Serial.println("[WIFI] -----> Could not connect to WIFI");
      delay(200);
      ESP.restart();
    }
  }
  Serial.println();
  Serial.println("[WIFI] ===> WiFi connected");
  Serial.print("[WIFI] ------> IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
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
  ir_data.nextTimercheck = ir_data.intervalon;

  lgWhisen.setActype(AC_CONF_TYPE);
  lgWhisen.setHeating(AC_CONF_HEATING);
  lgWhisen.setTemp(ir_data.ac_temp);    // 18 ~ 30
  lgWhisen.setFlow(ir_data.ac_flow);    // 0 : low, 1 : mid, 2 : high, if setActype == 1, 3 : change
  lgWhisen.setIrpin(IR_TX_PIN);         // ir tx pin

  irrecv.enableIRIn();


  // wifi connect
  wifi_connect();

  clientName = "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  // ntp update
  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet)
  {
    setSyncProvider(getNtpTime);
    delay(200);
  }

  // mqtt connect
  reconnect();

  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-slackbot");
  ArduinoOTA.setPassword(OTA_PASSWORD);
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
long lastReconnectAttempt = 0;
time_t prevDisplay = 0;

/**
  Sends a ping every 5 seconds, and handles reconnections
*/
void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (now() != prevDisplay)
    {
      prevDisplay = now();
      if (timeStatus() == timeSet)
      {
        digitalClockDisplay();
      }
    }

    if (!mqttclient.connected())
    {
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 100)
      {
        lastReconnectAttempt = now;
        if (reconnect())
        {
          lastReconnectAttempt = 0;
        }
      }
    }
    else
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
      { // Try to connect / reconnect to slack
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
        { // ac power down
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
        if (((millis() - ir_data.timerMillis) > ir_data.nextTimercheck) || !ir_data.timerfirst)
        {
          if (ir_data.ac_mode == 0)
          {
            ir_data.ac_mode = 1;
            ir_data.nextTimercheck = ir_data.intervalon;
          }
          else
          {
            ir_data.ac_mode = 0;
            ir_data.nextTimercheck = ir_data.intervaloff;
          }
          ir_data.haveData = true;
          ir_data.timerfirst  = true;
          ir_data.timerMillis = millis();
        }
      }
      mqttclient.loop();
    }
    ArduinoOTA.handle();
  }
  else
  {
    wifi_connect();
    setSyncProvider(getNtpTime);
  }
}

//----------------
String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(mqtt_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0;
}

void sendNTPpacket(IPAddress & address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void digitalClockDisplay()
{
  Serial.print("[TIME] ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());

  Serial.print(" ave.mean(): ");
  Serial.print(ave.mean());
  Serial.print(" ave.stddev(): ");
  Serial.println(ave.stddev());

}

void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
// end
