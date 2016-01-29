#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

#define DEBUG_PRINT 0

#define IPSET_STATIC { 192, 168, 10, 7 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

IPAddress ip_static = IPSET_STATIC;
IPAddress ip_gateway = IPSET_GATEWAY;
IPAddress ip_subnet = IPSET_SUBNET;
IPAddress ip_dns = IPSET_DNS;

unsigned int localPort = 2390;
const int timeZone = 0;

String macToStr(const uint8_t* mac);
void sendUdpSyslog(String msgtosend);
time_t getNtpTime();
void sendNTPpacket(IPAddress & address);
static unsigned long  numberOfSecondsSinceEpoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s);
long DateToMjd (uint16_t y, uint8_t m, uint8_t d);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress influxdbudp = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

String clientName;
unsigned long startMills;
unsigned long prevMills;
unsigned long portCount;

uint32_t timestamp;

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

/*
  #define LIMIT_RATE_MASK_ALL (0x03)
  #define FIXED_RATE_MASK_ALL (0x03)

  #define RC_LIMIT_11B    0
  #define RC_LIMIT_11G    1
  #define RC_LIMIT_11N    2
*/

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
  prevMills =  millis();
  portCount = 0;

  if (DEBUG_PRINT) {
    Serial.begin(74880);
  }

  Serial.println("");
  Serial.println("rtc mem test");
  Serial.println(wifi_station_get_auto_connect());
  WiFi.setAutoConnect(true);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  Serial.println(clientName);

  //
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  delay(50);
  setSyncProvider(getNtpTime);
  if (timeStatus() == timeNotSet) {
    Serial.println("waiting for sync message");
  }
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (timeStatus() != timeNotSet) {
      timestamp = numberOfSecondsSinceEpoch(year(), month(), day(), hour(), minute(), second());
      int millisnow = millisecond();
      
      Serial.print("epoch : ");
      Serial.println(timestamp);

      Serial.print("milis : ");
      Serial.println(millisecond());      

// 1434055562 005 000 035
// 14540691511
// 1454069151
// 000000

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
      payload += ",delay=";
      payload += (millis() - prevMills);
      payload += " ";
      payload += timestamp;
      if ( millisnow > 99 ) {
        payload += millisnow;
      } else if ( millisnow > 9 && millisnow < 100 ) {
        payload += "0";
        payload += millisnow;
      } else {
        payload += "00";
        payload += millisnow;
      }
      payload += "000000";

      Serial.print("payload : ");
      Serial.println(payload);
      sendUdpSyslog(payload);
      
    }
  } else {
    wifi_connect();
  }
  prevMills = millis() ;
  portCount++;
  delayMicroseconds(500);
  //delay(1);
}

void sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  if ( portCount % 2 == 0 ) {
    udp.beginPacket(influxdbudp, 8089);
  } else {
    udp.beginPacket(influxdbudp, 8090);
  }
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

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request called");

  sendNTPpacket(time_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println(millis() - beginWait);
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address)
{
  Serial.println("Transmit NTP Request");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  Serial.println("Transmit NTP Sent");
}
//

/*
const uint8_t daysInMonth [] PROGMEM = {
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
}; 

static unsigned long numberOfSecondsSinceEpoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s) {
  if (y >= 1970)
    y -= 1970;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0)
    ++days;
  days += 365 * y + (y + 3) / 4 - 1;
  return days * 24L * 3600L + h * 3600L + mm * 60L + s;
}
*/

long DateToMjd (uint16_t y, uint8_t m, uint8_t d)
{
  return
    367 * y
    - 7 * (y + (m + 9) / 12) / 4
    - 3 * ((y + (m - 9) / 7) / 100 + 1) / 4
    + 275 * m / 9
    + d
    + 1721028
    - 2400000;
}

static unsigned long  numberOfSecondsSinceEpoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s) 
{
  long Days;

  Days = DateToMjd(y, m, d) - DateToMjd(1970, 1, 1);
  return (uint16_t)Days * 86400 + h * 3600L + mm * 60L + s;
}



