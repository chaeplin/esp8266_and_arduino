// flash 4M, CPU 80Mhz
/*
  D5(SCL)   - pro mini / i2c
  D4(SDA)   - pro mini / i2c
  D13(MOSI) - LED
  D14(SCK)  - INT from pro mini
*/
#include <ESP8266WiFi.h>
#include <ESP8266Ping.h>
#include <PubSubClient.h>
#include <Average.h>
#include <pgmspace.h>
#include <Wire.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

#define nemoisOnPin 14
#define ledPin 13

#define DEBUG_PRINT 1

#define REPORT_INTERVAL 5000 // in msec

// ****************
//void callback(char* intopic, byte* inpayload, unsigned int length);
String macToStr(const uint8_t* mac);
void check_isr();
void hx711IsReady();
void sendHx711toMqtt(String payload, char* topic, int retain);
void sendUdpSyslog(String msgtosend);

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

const IPAddress influxdbudp = MQTT_SERVER;
const IPAddress mqtt_server = MQTT_SERVER;
const IPAddress time_server = MQTT_SERVER;

// ICMP
const IPAddress ap2(192, 168, 10, 2);
const IPAddress dns(8, 8, 8, 8);

bool ret_ap2_result, ret_dns_result;
int millis_ap2, millis_dns, pingloopcount;
String pingpayload;
String syslogPayload;

//-------------
int measured = 0;
int inuse = LOW;
int r = LOW;
int o_r = LOW;

volatile int m = LOW;
int o_m = LOW;

int AvgMeasuredIsSent = LOW;
int nofchecked        = 0;
int nofnotinuse       = 0;
int measured_poop     = 0;
int measured_empty    = 0;

Average<float> ave(10);

char* topicEvery    = "esp8266/arduino/s16";
char* topicAverage  = "esp8266/arduino/s06";
char* topicpingtest = "esp8266/ping";
char* hellotopic    = "HELLO";

char* willTopic = "clients/scale";
char* willMessage = "0";

String clientName;
String payload;

// send reset info
String getResetInfo ;
int ResetInfo = LOW;

WiFiClient wifiClient;
WiFiUDP udp;
PubSubClient client(mqtt_server, 1883, wifiClient);

unsigned long startMills, sentMills;

long lastReconnectAttempt = 0;

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-scale");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 200) {
      ESP.restart();
    }
  }
}

boolean reconnect() {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
      client.publish(willTopic, "1", true);
      if ( ResetInfo == LOW) {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        ResetInfo = HIGH;
      } else {
        client.publish(hellotopic, "hello again 1 from scale");
      }
      if (DEBUG_PRINT) {
        sendUdpSyslog("connected");
      }
    } else {
      if (DEBUG_PRINT) {
        syslogPayload = "failed, rc=";
        syslogPayload += client.state();
        sendUdpSyslog(syslogPayload);
      }
    }
  }
  client.loop();
  return client.connected();
}

void setup() {
  system_update_cpu_freq(SYS_CPU_80MHz);

  Wire.begin(4, 5);
  delay(100);

  startMills = sentMills = millis();
  millis_ap2 = millis_dns = pingloopcount = 0;
  ret_ap2_result = ret_dns_result = false;

  pinMode(nemoisOnPin, INPUT);
  pinMode(ledPin, OUTPUT);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;

  getResetInfo = "hello from scale ";
  getResetInfo += ESP.getResetInfo().substring(0, 40);

  attachInterrupt(14, check_isr, RISING);

  //OTA
  ArduinoOTA.setHostname("esp-scale");
  ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]() {
    sendUdpSyslog("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]() {
    sendUdpSyslog("ArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    syslogPayload = "Progress: ";
    syslogPayload += (progress / (total / 100));
    sendUdpSyslog(syslogPayload);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //ESP.restart();
    if (error == OTA_AUTH_ERROR) abort();
    else if (error == OTA_BEGIN_ERROR) abort();
    else if (error == OTA_CONNECT_ERROR) abort();
    else if (error == OTA_RECEIVE_ERROR) abort();
    else if (error == OTA_END_ERROR) abort();

  });

  ArduinoOTA.begin();
  sendUdpSyslog("-------------> scale started"); 
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (DEBUG_PRINT) {
        syslogPayload = "failed, rc= ";
        syslogPayload += client.state();
        sendUdpSyslog(syslogPayload);
      }
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 1000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {

      if ( m != o_m ) {
        sendUdpSyslog("hx711IsReady : i2c read");
        detachInterrupt(14);
        hx711IsReady();
        sendUdpSyslog("hx711IsReady : i2c read done");
        attachInterrupt(14, check_isr, RISING);
        o_m = m;
      }

      if ((millis() - sentMills) > REPORT_INTERVAL ) {
        if ( m == o_m && inuse == LOW ) {
          switch (pingloopcount) {
            case 0:
              detachInterrupt(14);
              sendUdpSyslog("ap2 ping start");

              ESP.wdtDisable();
              ret_ap2_result = Ping.ping(ap2, 1);
              ESP.wdtEnable(2000);
              if (ret_ap2_result) {
                millis_ap2 = Ping.averageTime();
              } else {
                millis_ap2 = 0;
              }
              sentMills = millis();
              pingloopcount++;

              sendUdpSyslog("ap2 ping stop");
              attachInterrupt(14, check_isr, RISING);
              break;

            case 1:
              detachInterrupt(14);
              sendUdpSyslog("dns ping start");

              ESP.wdtDisable();
              ret_dns_result = Ping.ping(dns, 1);
              ESP.wdtEnable(2000);
              if (ret_dns_result) {
                millis_dns = Ping.averageTime();
              } else {
                millis_dns = 0;
              }
              sentMills = millis();
              pingloopcount++;

              sendUdpSyslog("dns ping stop");
              attachInterrupt(14, check_isr, RISING);
              break;

            case 2:
              detachInterrupt(14);
              sendUdpSyslog("ping case 2 start");
              pingpayload  = "{\"ap2\":";
              pingpayload += millis_ap2;
              pingpayload += ",\"dns\":";
              pingpayload += millis_dns;
              pingpayload += ",\"ret_ap2_result\":";
              pingpayload += ret_ap2_result;
              pingpayload += ",\"ret_dns_result\":";
              pingpayload += ret_dns_result;
              pingpayload += "}";

              sendHx711toMqtt(pingpayload, topicpingtest, 0);
              sentMills = millis();
              pingloopcount = 0;

              sendUdpSyslog("ping case 2 stop");
              attachInterrupt(14, check_isr, RISING);
              break;
          }
        }
      }

      if ( r != o_r ) {
        ave.push(measured);

        if ( inuse == HIGH ) {
          digitalWrite(ledPin, HIGH);
          if ( measured > 200 ) {
            if ( ( ave.stddev() < 15 ) && ( nofchecked > 15 ) && ( ave.mean() > 1000 ) && ( ave.mean() < 7000 ) && ( AvgMeasuredIsSent == LOW ) ) {
              payload = "{\"WeightAvg\":";
              payload += ( int(ave.mean()) - measured_empty );
              payload += ",\"WeightStddev\":";
              payload += ave.stddev();
              payload += "}";

              sendHx711toMqtt(payload, topicAverage, 1);
            } else {
              if ( nofchecked > 3 ) {
                payload = "{\"NemoWeight\":";
                payload += ( measured - measured_empty );
                payload += "}";

                sendHx711toMqtt(payload, topicEvery, 0);
              }
            }
          }
        } else {
          digitalWrite(ledPin, LOW);

          if ( ( ave.stddev() < 10 ) && ( nofnotinuse > 20 ) ) {
            if ( AvgMeasuredIsSent == HIGH ) {
              if (DEBUG_PRINT) {
                syslogPayload = "poop_checked : ";
                syslogPayload += int(ave.mean());
                sendUdpSyslog(syslogPayload);
              }

              payload = "{\"WeightPoop\":";
              payload += ( int(ave.mean()) - measured_empty );
              payload += ",\"NemoWeight\":0}";
              sendHx711toMqtt(payload, topicEvery, 0);
              /*
                        payload = "{\"WeightAvg\":0}";
                        sendHx711toMqtt(payload, topicAverage, 0);
              */
              AvgMeasuredIsSent = LOW;
            } else {
              payload = "{\"NemoEmpty\":";
              payload += int(ave.mean());
              payload += ",\"NemoStddev\":";
              payload += ave.stddev();
              payload += ",\"FreeHeap\":";
              payload += ESP.getFreeHeap();
              payload += ",\"RSSI\":";
              payload += WiFi.RSSI();
              payload += ",\"millis\":";
              payload += (millis() - startMills);
              payload += "}";

              measured_empty = int(ave.mean());
              sendHx711toMqtt(payload, topicEvery, 0);
            }
            nofnotinuse = 0;
          }

          nofchecked = 0;
        }
        nofchecked++;
        nofnotinuse++;
        o_r = r;
      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}

void ICACHE_RAM_ATTR check_isr() {
  m = !m;
}

void ICACHE_RAM_ATTR sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-scale: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR hx711IsReady() {

  Wire.requestFrom(2, 3);

  int x;
  byte a, b, c;

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();

  // nedd to check value of a, b, c
  // result of Wire.read

  x = a;
  x = x << 8 | b;

  if ( x >= 7000 ) {
    x = 0;
  }

  if ( c == 1 ) {
    x = x * -1;
  }

  if ( x > 200 ) {
    inuse = HIGH;
  } else {
    inuse = LOW;
  }
  measured = x;
  r = !r;
}

void ICACHE_RAM_ATTR sendHx711toMqtt(String payload, char* topic, int retain) {
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if ( retain == 1 ) {
    if ( client.publish(topic, p, msg_length, 1)) {
      if ( topic == "esp8266/arduino/s06" ) {
        AvgMeasuredIsSent = HIGH;
      }
      client.loop();
      free(p);
    } else {
      if (DEBUG_PRINT) {
        syslogPayload = topic;
        syslogPayload += " - ";
        syslogPayload += payload;
        syslogPayload += " : Publish fail";
        sendUdpSyslog(syslogPayload);
      }
      client.loop();
      free(p);
    }
  } else {
    if ( client.publish(topic, p, msg_length)) {
      if ( topic == "esp8266/arduino/s06" ) {
        AvgMeasuredIsSent = HIGH;
      }
      client.loop();
      free(p);
    } else {
      if (DEBUG_PRINT) {
        syslogPayload = topic;
        syslogPayload += " - ";
        syslogPayload += payload;
        syslogPayload += " : Publish fail";
        sendUdpSyslog(syslogPayload);
      }
      client.loop();
      free(p);
    }
  }

}

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}
