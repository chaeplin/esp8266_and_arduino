#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

#define IPSET_STATIC { 192, 168, 10, 7 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

IPAddress ip_static = IPSET_STATIC;
IPAddress ip_gateway = IPSET_GATEWAY;
IPAddress ip_subnet = IPSET_SUBNET;
IPAddress ip_dns = IPSET_DNS;

unsigned int localPort = 2390;

String macToStr(const uint8_t* mac);
void sendUdpSyslog(String msgtosend);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress influxdbudp = MQTT_SERVER;

String clientName;
unsigned long startMills;

WiFiClient wifiClient;
WiFiUDP udp;

/**
   set the output power of WiFi
   @param dBm max: +20.5dBm  min: 0dBm
*/
/*
  void ESP8266WiFiGenericClass::setOutputPower(float dBm) {

    if(dBm > 20.5) {
        dBm = 20.5;
    } else if(dBm < 0) {
        dBm = 0;
    }

    uint8_t val = (dBm*4.0f);
    system_phy_set_max_tpw(val);
  }
*/

/*
  typedef enum {
    WIFI_PHY_MODE_11B = 1, WIFI_PHY_MODE_11G = 2, WIFI_PHY_MODE_11N = 3
  } WiFiPhyMode_t;

  bool ESP8266WiFiGenericClass::setPhyMode(WiFiPhyMode_t mode) {
    return wifi_set_phy_mode((phy_mode_t) mode);
  }
*/

#define LIMIT_RATE_MASK_ALL (0x03)
#define FIXED_RATE_MASK_ALL (0x03)

#define RC_LIMIT_11B    0
#define RC_LIMIT_11G    1
#define RC_LIMIT_11N    2

/*
  bool wifi_set_user_limit_rate_mask(uint8 enable_mask)

  int wifi_set_user_fixed_rate(uint8 enable_mask, uint8 rate)

  enum FIXED_RATE {
        PHY_RATE_48       = 0x8,
        PHY_RATE_24       = 0x9,
        PHY_RATE_12       = 0xA,
        PHY_RATE_6        = 0xB,
        PHY_RATE_54       = 0xC,
        PHY_RATE_36       = 0xD,
        PHY_RATE_18       = 0xE,
        PHY_RATE_9        = 0xF,
  };

  enum RATE_11G_ID {
  RATE_11G_G54M = 0,
  RATE_11G_G48M = 1,
  RATE_11G_G36M = 2,
  RATE_11G_G24M = 3,
  RATE_11G_G18M = 4,
  RATE_11G_G12M = 5,
  RATE_11G_G9M  = 6,
  RATE_11G_G6M  = 7,
  RATE_11G_B5M  = 8,
  RATE_11G_B2M  = 9,
  RATE_11G_B1M  = 10
  };

  enum RATE_11N_ID {
  RATE_11N_MCS7S  = 0,
  RATE_11N_MCS7 = 1,
  RATE_11N_MCS6 = 2,
  RATE_11N_MCS5 = 3,
  RATE_11N_MCS4 = 4,
  RATE_11N_MCS3 = 5,
  RATE_11N_MCS2 = 6,
  RATE_11N_MCS1 = 7,
  RATE_11N_MCS0 = 8,
  RATE_11N_B5M  = 9,
  RATE_11N_B2M  = 10,
  RATE_11N_B1M  = 11
  };
  #define RC_LIMIT_11B    0
  #define RC_LIMIT_11G    1
  #define RC_LIMIT_11N    2

  bool wifi_set_user_rate_limit(uint8 mode, uint8 ifidx, uint8 max, uint8 min)

*/



void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED) {
    // WIFI
    Serial.println();
    Serial.print("===> WIFI ---> Connecting to ");
    Serial.println(ssid);
    delay(10);

    /*
        WiFi.setOutputPower(13);
        WiFi.setPhyMode(WIFI_PHY_MODE_11G);
        wifi_set_user_limit_rate_mask(LIMIT_RATE_MASK_ALL);
        wifi_set_user_fixed_rate(FIXED_RATE_MASK_ALL, PHY_RATE_6);
    */
    //wifi_set_user_rate_limit(RC_LIMIT_11N, 0x00, RATE_11N_B5M, RATE_11N_B1M);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    //WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));


    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(". ");
      Serial.print(Attempt);
      delay(100);
      Attempt++;
      if (Attempt == 250)
      {
        Serial.println();
        Serial.println("-----> Could not connect to WIFI");
        ESP.restart();
        delay(200);
      }
    }
    Serial.println();
    Serial.print("===> WiFi connected");
    Serial.print(" ------> IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup()
{
  startMills = millis();
  //Serial.begin(74880);

  Serial.println("");
  Serial.println("rtc mem test");
  Serial.println(wifi_station_get_auto_connect());
  WiFi.setAutoConnect(true);

  wifi_connect();

  udp.begin(localPort);

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  Serial.println(clientName);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    uint32_t really_random = *(volatile uint32_t *)0x3FF20E44;
    String payload = "udptest,test=test01 ";
    payload += "startMills=";
    payload += (millis() - startMills);
    payload += "i,rand=";
    payload += really_random;
    payload += ",FreeHeap=";
    payload += ESP.getFreeHeap();
    payload += "i,RSSI=";
    payload += WiFi.RSSI();

    Serial.print("payload : ");
    Serial.println(payload);
    sendUdpSyslog(payload);

  } else {
    wifi_connect();
  }
  //delay(1);
  delayMicroseconds(200);
}

void sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

