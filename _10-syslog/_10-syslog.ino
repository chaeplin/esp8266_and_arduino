#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define DEBUG_PRINT 0

unsigned int localPort = 2390;
IPAddress syslogServer(192, 168, 10, 10);

String str;
//long startMills;

WiFiClient wifiClient;
WiFiUDP udp;

void wifi_connect() {

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (DEBUG_PRINT) {
      Serial.print(".");
    }
    if (Attempt == 200)
    {
      ESP.restart();
    }
  }
  if (DEBUG_PRINT) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup()
{
  Serial.begin(38400);
  delay(20);
  wifi_connect();
  udp.begin(localPort);
  delay(500);
  if (WiFi.status() == WL_CONNECTED) {
    sendUdpSyslog("esp8266-syslog started");
  }
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    str = Serial.readStringUntil('\n');
    str.trim();
    unsigned int msg_length = str.length();
    if ( msg_length > 0 ) {
      sendUdpSyslog(str);
    }
  } else {
    wifi_connect();
  }
}

void sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(syslogServer, 514);
  udp.write("esp8266-syslog ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}
