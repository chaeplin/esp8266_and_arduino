// esp8266/nestbridge
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

#define COOL_PIN 5
#define HEAT_PIN 4

#include "/usr/local/src/aptls_setting.h"
const char* reporting_topic = "esp8266/nestbridge";
const char* topic_ac_cmd    = "esp8266/cmd/ac";
const char* hellotopic      = "HELLO";

IPAddress mqtt_server = MQTT_SERVER;

String clientName;
long lastReconnectAttempt = 0;

unsigned int localPort = 12390;
const int timeZone = 9;
time_t prevDisplay = 0;

// send reset info
String getResetInfo;
int ResetInfo = LOW;

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length);
WiFiClientSecure sslclient;
PubSubClient client(mqtt_server, 8883, callback, sslclient);
WiFiUDP udp;

volatile bool bnest_isr_cool = false;
volatile bool bnest_isr_heat = false;

void ICACHE_RAM_ATTR nest_isr_cool() {
  bnest_isr_cool = true;
}

void ICACHE_RAM_ATTR nest_isr_heat() {
  bnest_isr_heat = true;
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

    Serial.print("[MQTT] out topic : ");
    Serial.print(topictosend);
    Serial.print(" payload: ");
    Serial.print(payloadtosend);
    Serial.println(" published");

    return true;

  } else {
    free(p);
    client.loop();

    Serial.print("[MQTT] out topic : ");
    Serial.print(topictosend);
    Serial.print(" payload: ");
    Serial.print(payloadtosend);
    Serial.println(" publish failed");

    return false;
  }
}

void ICACHE_RAM_ATTR sendCheck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["cool"] = digitalRead(COOL_PIN);
  root["heat"] = digitalRead(HEAT_PIN);
  String json;
  root.printTo(json);

  sendmqttMsg(reporting_topic, json, false);
}

void ICACHE_RAM_ATTR send_ac_cmd()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["AC"] = !digitalRead(COOL_PIN);
  String json;
  root.printTo(json);

  sendmqttMsg(topic_ac_cmd, json, true);
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
{
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
            client.publish(hellotopic, "hello again 1 from nestbridge");
         }
         Serial.println("[MQTT] mqtt connected");
      }
      else
      {
         Serial.print("[MQTT] mqtt failed, rc=");
         Serial.println(client.state());
      }
   }
  }
  return client.connected();
}

void ArduinoOTA_config()
{
  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-nestbridge");
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

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

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

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(mqtt_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500)
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

void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
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
  Serial.println(year());
}

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println();
    Serial.print("[WIFI] Connecting to ");
    Serial.println(WIFI_SSID);

    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.hostname("esp-nestbridge");

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(". ");
      Serial.print(Attempt);
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        Serial.println();
        Serial.println("[WIFI] Could not connect to WIFI, restarting...");
        Serial.flush();
        ESP.restart();
        delay(200);
      }
    }

    Serial.println();
    Serial.print("[WIFI] connected");
    Serial.print(" --> IP address: ");
    Serial.println(WiFi.localIP());
  }
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

void setup()
{
   Serial.begin(115200);
   Serial.println();
   Serial.println("Starting....... ");

   pinMode(COOL_PIN, INPUT);
   pinMode(HEAT_PIN, INPUT);

   attachInterrupt(COOL_PIN, nest_isr_cool, CHANGE);
   attachInterrupt(HEAT_PIN, nest_isr_heat, CHANGE);

   wifi_connect();

   clientName += "esp8266-";
   uint8_t mac[6];
   WiFi.macAddress(mac);
   clientName += macToStr(mac);
   clientName += "-";
   clientName += String(micros() & 0xff, 16);

   getResetInfo = "hello from nestbridge ";
   getResetInfo += ESP.getResetInfo().substring(0, 50);

   configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

   // ntp update
   udp.begin(localPort);
   if (timeStatus() == timeNotSet)
   {
     Serial.println("[NTP] get ntp time");
     setSyncProvider(getNtpTime);
     delay(500);
   }

   ArduinoOTA_config();

   reconnect();
   lastReconnectAttempt = 0;
   sendCheck();
}

void loop()
{
  if (now() != prevDisplay)
  {
    prevDisplay = now();
    if (timeStatus() == timeSet)
    {
      digitalClockDisplay();
    }
    else
    {
      Serial.println("[TIME] time not set");
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      long now = millis();
      if (now - lastReconnectAttempt > 5000)
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
      if (bnest_isr_cool)
      {
         sendCheck();
         send_ac_cmd();
         bnest_isr_cool = false;
      }      

      if (bnest_isr_heat)
      {
         sendCheck();
         bnest_isr_heat = false;
      }

      client.loop();
    }
    ArduinoOTA.handle();
  }
  else
  {
    wifi_connect();
  }
}
