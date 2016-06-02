// https://github.com/PaulStoffregen/Time
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <esp8266Twitter.h>

extern "C" {
#include "user_interface.h"
}

/* -- */
#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/gopro_setting.h"
#include "/usr/local/src/twitter_setting.h"


const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress time_server = MQTT_SERVER;
// twitter
const char* consumer_key    = ConsumerKey;
const char* consumer_secret = ConsumerSecret;
const char* access_token    = AccessToken;
const char* access_secret   = AccessSecret;

WiFiClientSecure client;
WiFiUDP udp;

unsigned int localPort = 12390;
const int timeZone     = 9;

bool x;
int y;

// 
esp8266Twitter esp8266Twitter(consumer_key, consumer_secret, access_token, access_secret);

void wifi_connect() {
  Serial.print("[WIFI] start millis     : ");
  Serial.println(millis());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300) {
      ESP.restart();
    }
  }
  Serial.print("[WIFI] connected millis : ");
  Serial.print(millis());
  Serial.print(" - ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }
  Serial.println();

  wifi_connect();

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    setSyncProvider(getNtpTime);
  }

  x = true;
  y = 0;
}

time_t prevDisplay = 0;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      if (timeStatus() == timeSet) {
        digitalClockDisplay();

        if (x && y == 10) {
          Serial.print("[TWEET] tweet :  ");
          tweeting();
          x = false;
        }
      }
      y++;
    }
  } else {
    wifi_connect();
    setSyncProvider(getNtpTime);
  }
}

void tweeting() {
  const char* value_status  = "esp-01 + direct tweet / test 07";
  uint32_t value_timestamp  = now();
  uint32_t value_nonce      = *(volatile uint32_t *)0x3FF20E44;

  Serial.println(value_status);
  if (esp8266Twitter.tweet(value_status, value_timestamp, value_nonce)) {
    Serial.println("[TWEET] result :  ok");
  } else {
    Serial.println("[TWEET] tweet :  failed");
  }
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime() {
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(time_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
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

void sendNTPpacket(IPAddress & address) {
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


void digitalClockDisplay() {
  Serial.print("[TIME] ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.println(year());
}

void printDigits(int digits) {
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// end
