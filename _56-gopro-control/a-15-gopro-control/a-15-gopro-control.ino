// esp-01 1M / 64K flash
#include <Arduino.h>
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <base64.h>
#include <FS.h>


/* -- */
#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/gopro_setting.h"
#include "/usr/local/src/twitter_setting.h"
/* -- */

const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASSWORD;
const char* goprossid     = GOPRO_SSID;
const char* gopropassword = GOPRO_PASSWORD;
const char* otapassword   = OTA_PASSWORD;


extern "C" {
#include "user_interface.h"
}


/////////////
WiFiClient wifiClient;
WiFiClientSecure sslclient;

bool x;

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println("[WIFIE] connected. IP address: " + String(WiFi.localIP().toString()) + " hostname: " + WiFi.hostname() + "  SSID: " + WiFi.SSID());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("[WIFIE] client lost connection");
      break;
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.println("[WIFIE] client connected");
      break;
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
      Serial.println("[WIFIE] client authentication mode changed.");
      break;
    //case WIFI_STAMODE_DHCP_TIMEOUT:  THIS IS A NEW CONSTANT ENABLE WITH UPDATED SDK
    //  Serial.println("[WIFIE] client DHCP timeout reached.");
    //break;
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      Serial.println("[WIFIE] accesspoint: new client connected. Clients: "  + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.println("[WIFIE] accesspoint: client disconnected. Clients: " + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      //Serial.println("[WIFIE] accesspoint: probe request received.");
      break;
  }
}

void gopro_connect() {
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(goprossid, gopropassword);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);
  WiFi.onEvent(WiFiEvent);
  gopro_connect();
  x = true;
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (x) {
      gopro_poweron();
      x = false;
    }
  }
}


bool gopro_poweron() {
  bool rtn = false;
  HTTPClient http;
  /*
    String url = "http://10.5.5.9/bacpac/PW?t=";
    url += gopropassword;
    url += "&p=%01";

    lcd.setCursor(0, 1);

    http.begin(url);
  */
  http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%01");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  } else {
    rtn = false;
  }
  http.end();
  return rtn;
}
