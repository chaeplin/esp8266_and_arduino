#include <ESP8266WiFi.h>
#include <time.h>

#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define DEBUG_PRINT 1

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
//int32_t channel = WIFI_CHANNEL;
//byte bssid[] = WIFI_BSSID;
byte mqtt_server[] = MQTT_SERVER;
byte ip_static[] = IP_STATIC;
byte ip_gateway[] = IP_GATEWAY;
byte ip_subnet[] = IP_SUBNET;
// ****************

int timezone = 9;
int dst = 0;

WiFiClient wifiClient;

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED) {
    // WIFI
    if (DEBUG_PRINT) {
      Serial.print("===> WIFI ---> Connecting to ");
      Serial.println(ssid);
    }
    delay(10);
    WiFi.mode(WIFI_STA);
    // ****************
    WiFi.begin(ssid, password);
    //WiFi.begin(ssid, password, channel, bssid);
    //WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet));
    // ****************

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (DEBUG_PRINT) {
        Serial.print(". ");
        Serial.print(Attempt);
      }
      delay(100);
      //
      Attempt++;
      if (Attempt == 150)
      {
        if (DEBUG_PRINT) {
          Serial.println();
          Serial.println("-----> Could not connect to WIFI");
        }
      }
    }


    if (DEBUG_PRINT) {
      Serial.println();
      Serial.print("===> WiFi connected : ");
      Serial.print("---> IP address: ");
      Serial.println(WiFi.localIP());

      Serial.print("===>  Current WIFI : ");
      Serial.print(WiFi.RSSI());
      Serial.print("\t");
      Serial.print(WiFi.BSSIDstr());
      Serial.print("\t");
      Serial.print(WiFi.channel());
      Serial.print("\t");
      Serial.println(WiFi.SSID());

    }

  }
}

void setup()
{

  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }

  wifi_connect();

  configTime(timezone * 3600, dst, "192.168.10.10");
  Serial.println("\nWaiting for time");
  while (!time(nullptr)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("");

}

void loop()
{

  time_t now = time(nullptr);
  Serial.println(ctime(&now));
  delay(1000);

}

