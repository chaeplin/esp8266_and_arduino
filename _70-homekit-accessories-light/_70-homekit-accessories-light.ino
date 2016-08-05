// Using Sonoff wifi switch, 80MHz, 1M
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "/usr/local/src/aptls_setting.h"

extern "C" {
#include "user_interface.h"
}

#define BUTTON_PIN 0
#define RELAY_PIN 12
#define LED_PIN 13

#define BETWEEN_RELAY_ACTIVE 1000
#define REPORT_INTERVAL 5000 // in msec

IPAddress mqtt_server = MQTT_SERVER;

const char* subscribe_topic = "light/bedroomlight";
const char* reporting_topic = "light/bedroomlight/report";
const char* status_topic    = "light/bedroomlight/status";
const char* hellotopic      = "HELLO";

long lastReconnectAttempt = 0;
volatile bool bUpdated = false;
volatile bool bRelayState = false;
volatile bool bRelayReady = false; 
String clientName;
unsigned long lastRelayActionmillis;
unsigned long startMills;

// send reset info
String getResetInfo;
int ResetInfo = LOW;

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length);

WiFiClientSecure sslclient;
PubSubClient client(mqtt_server, 8883, callback, sslclient);
Ticker ticker;

void tick()
{
  //toggle state
  int state = digitalRead(LED_PIN);  // get the current state of GPIO13 pin
  digitalWrite(LED_PIN, !state);     // set pin to the opposite state
}

bool ICACHE_RAM_ATTR sendmqttMsg(const char* topictosend, String payloadtosend, bool retain = false)
{
  unsigned int msg_length = payloadtosend.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payloadtosend.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, retain))
  {
    free(p);
    client.loop();
    return true;

  } else {
    free(p);
    client.loop();
    return false;
  }
}

void ICACHE_RAM_ATTR sendreport()
{
  String payload;
  if (bRelayState)
  {
      payload = "true";
  }
  else
  {
      payload = "false";
  }
  sendmqttMsg(reporting_topic, payload, true);
}

void ICACHE_RAM_ATTR sendCheck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["FreeHeap"] = ESP.getFreeHeap();
  root["RSSI"]     = WiFi.RSSI();
  root["millis"]   = millis();
  String json;
  root.printTo(json);

  sendmqttMsg(status_topic, json, false);
}

void ICACHE_RAM_ATTR parseMqttMsg(String receivedpayload, String receivedtopic)
{
  if (receivedtopic == subscribe_topic)
  {
    if (bRelayReady)
    {
      if ( receivedpayload == "true")
      {
        bRelayState = HIGH;
      }
      else
      {
        bRelayState = LOW;
      }
      bUpdated = true;
    }
    else
    {
      sendreport();
    }
  }   
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (unsigned int i = 0; i < length; i++)
  {
    receivedpayload += (char)inpayload[i];
  }

  parseMqttMsg(receivedpayload, receivedtopic);
}

bool verifytls()
{

  if (!sslclient.connect(mqtt_server, 8883))
  {
    return false;
  }

  if (sslclient.verify(MQTT_FINGERPRINT, MQTT_SERVER_CN))
  {
    sslclient.stop();
    return true;
  }
  else
  {
    sslclient.stop();
    return false;
  }
}

boolean reconnect()
{
  if (!client.connected())
  {
    if (verifytls())
    {
      if (client.connect((char*) clientName.c_str(), MQTT_USER, MQTT_PASS))
      {
         if ( ResetInfo == LOW) {
            client.publish(hellotopic, (char*) getResetInfo.c_str());
            ResetInfo = HIGH;
         } else {
            client.publish(hellotopic, "hello again 1 from bedroomlight");
         }

        client.subscribe(subscribe_topic);
        client.loop();
        ticker.attach(1, tick);
      }
      else
      {
        ticker.attach(0.5, tick);
      }
    }
  }
  return client.connected();
}

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    wifi_set_phy_mode(PHY_MODE_11N);
    WiFi.setOutputPower(20);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.hostname("esp-bedroomlight");

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        ESP.restart();
        delay(200);
      }
    }
  }
}

void change_light()
{
  digitalWrite(RELAY_PIN, bRelayState);
}

void run_lightcmd_isr()
{
  bRelayState = !bRelayState;
  bUpdated = true;
}

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

void ArduinoOTA_config()
{
  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-bedroomlight");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() 
  { 
     //ticker.attach(0.1, tick); 
  });
  ArduinoOTA.onEnd([]() { });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { });
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

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  startMills = lastRelayActionmillis = millis();

  attachInterrupt(BUTTON_PIN, run_lightcmd_isr, RISING);

  ticker.attach(0.2, tick);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  getResetInfo = "hello from bedroomlight ";
  getResetInfo += ESP.getResetInfo().substring(0, 50);

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  ArduinoOTA_config();
  
  reconnect();
  lastReconnectAttempt = 0;
}

void loop()
{
  if ((millis() - lastRelayActionmillis) > BETWEEN_RELAY_ACTIVE)
  {
    bRelayReady = true;
  }
  else
  {
    bRelayReady = false;
  }

  if (bUpdated)
  {
    if (bRelayReady)
    {
      change_light();
      if (client.connected())
      {
         sendreport();
      }
      lastRelayActionmillis = millis();
      bUpdated = false;
    }
    else
    {
      bUpdated = false;
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      long now = millis();
      if (now - lastReconnectAttempt > 1000)
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
      if (millis() - startMills > REPORT_INTERVAL)
      {
         sendCheck();
         startMills = millis();
      }
      client.loop();
    }
    ArduinoOTA.handle();
  }
  else
  {
    ticker.attach(0.2, tick);
    wifi_connect();
  }
}
// end of file
