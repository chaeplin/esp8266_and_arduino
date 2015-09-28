#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Time.h>


#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif


char* topic   = "esp8266/arduino/s20";
char* hellotopic = "HELLO";

char* willTopic = "clients/s20";
char* willMessage = "0";

String clientName;
String payload;

WiFiClient wifiClient;
IPAddress server(192, 168, 10, 10);
PubSubClient client(wifiClient);
WiFiUDP udp;

unsigned int localPort = 2390;  // local port to listen for UDP packets
IPAddress timeServer(192, 168, 10, 10); // time.nist.gov NTP server
const int timeZone = 9;

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

unsigned long startMills;

long lastReconnectAttempt = 0;

void wifi_connect() {
  // WIFI
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(192, 168, 10, 112), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));


  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    Serial.print(". ");
    Serial.print(Attempt);
    if (Attempt == 100)
    {
      Serial.println();
      Serial.println("------------------------------> Could not connect to WIFI");
      ESP.restart();
      delay(200);
    }
  }

  Serial.println();
  Serial.println("===> WiFi connected");
  Serial.print("---------------------------------> IP address: ");
  Serial.println(WiFi.localIP());

/*
  //
  //Serial.println("Starting UDP");
  udp.begin(localPort);
  //Serial.print("Local port: ");
  //Serial.println(udp.localPort());
  delay(5000);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    Serial.println("waiting for sync message");
  }  

*/  

/*
  //startMills = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(willTopic, "1", true);
        //client.publish(hellotopic, "hello again wifi and mqtt from ESP8266 s20");
        Serial.println("reconnecting wifi and mqtt");
      } else {
        Serial.println("mqtt publish fail after wifi reconnect");
      }
    } else {
      client.publish(hellotopic, "hello again wifi from ESP8266 s20");
    }
  }
*/
}

boolean reconnect() {
  client.disconnect ();
  delay(100);  
  if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
    Serial.println("-----> mqtt connected");
    //client.publish(hellotopic, "hello again 1 from ESP8266 s20");
    client.publish(willTopic, "1", true);
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
  //startMills = millis();
  return client.connected();
}

void setup() {
  Serial.begin(38400);
  Serial.println();
  //Serial.println("lwt test");
  //Serial.print("ESP.getFlashChipSize() : ");
  //Serial.println(ESP.getFlashChipSize());
  delay(100);

  startMills = millis();

  wifi_connect();
  
  client.setServer(server, 1883);
  
  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;


  String getResetInfo = "hello from ESP8266 s20 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        //client.publish(hellotopic, (char*) getResetInfo.c_str());
        client.publish(willTopic, "1", true);
        //Serial.print("Sending payload: ");
        //Serial.println(getResetInfo);
      }
    } else {
      //client.publish(hellotopic, (char*) getResetInfo.c_str());
      client.publish(willTopic, "1", true);
      //Serial.print("Sending payload: ");
      //Serial.println(getResetInfo);
    }
  }

}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
  } else {
    wifi_connect();
  }

  client.loop();
  delay(2000);
  //Serial.println();
  //Serial.println("doing reset");
  ESP.restart();
  delay(200);
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
  //Serial.println("------------------> Transmit NTP Request called");
  sendNTPpacket(timeServer);
  delay(1000);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.print("---------------> Receive NTP Response :  ");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      Serial.println(secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR);
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.print("=================> No NTP Response :-( : ");
  Serial.println(millis() - beginWait);
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address)
{
  //Serial.println("Transmit NTP Request");
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
  Serial.println("------------> Transmit NTP Sent");
}
