// CPU : 80MHz
// FLASH : 4M/1M
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
// https://github.com/openenergymonitor/EmonLib
#include "EmonLib.h" // Include Emon Library
#include <Average.h>

extern "C" {
#include "user_interface.h"
}

#define _IS_MY_HOME
// WIFI
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif


#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

#define RTC_MAGIC 12345678

typedef struct _tagPoint {
  uint32 magic;
  uint32 salt;
} RTC_REVCOUNT;

RTC_REVCOUNT rtc_mem_revcount;

#define INFO_PRINT 1
#define DEBUG_PRINT 1

// ****************
void sendmqttMsg(char* topictosend, String payloadtosend);
void callback(char* intopic, byte* inpayload, unsigned int length);
String macToStr(const uint8_t* mac);
void IRCHECKING_START();
void DOORCHECKING();
void count_powermeter();
void sendUdpSyslog(String msgtosend);

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

//
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;

//
EnergyMonitor emon1;
Average<float> ave(4);

// mqtt
char* topic = "esp8266/arduino/s07";
char* doortopic = "esp8266/arduino/s05" ;
char* hellotopic = "HELLO";

char* willTopic = "clients/power";
char* willMessage = "0";

char* subtopic = "esp8266/check";

// pin : using line tracker
#define IRPIN 4
#define DOORPIN 5

#define REPORT_INTERVAL 1000 // in msec

volatile unsigned long startMills ;
volatile unsigned long revMills ;

unsigned long timemillis;

unsigned long oldrevMills ;
unsigned long sentMills ;
volatile unsigned long revCounts ;

volatile int irStatus = LOW ;

int oldirStatus ;

float revValue ;
float oldrevValue ;
double VIrms ;

// door
volatile int doorStatus ;
int olddoorStatus ;

//
String clientName ;
String payload ;
String doorpayload ;
String syslogPayload;

// send reset info
String getResetInfo ;
int ResetInfo = LOW;

int average = 0;

WiFiClient wifiClient;
WiFiUDP udp;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

long lastReconnectAttempt = 0;
long calcIrmsmillis = 0;

void ICACHE_RAM_ATTR rtc_check() {
  // system_rtc_mem_read(64... not work, use > 64
  system_rtc_mem_read(100, &rtc_mem_revcount, sizeof(rtc_mem_revcount));
  if (rtc_mem_revcount.magic != RTC_MAGIC) {
    rtc_mem_revcount.magic = RTC_MAGIC;
    rtc_mem_revcount.salt = 5255;
  }
}

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-power");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 200)
    {
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
        client.publish(hellotopic, "hello again 1 from esp-power");
      }
      client.subscribe(subtopic);
      if (DEBUG_PRINT) {
        sendUdpSyslog("---> mqttconnected");
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

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length) {
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  if (INFO_PRINT) {
    syslogPayload = intopic;
    syslogPayload += " ====> ";
    syslogPayload += receivedpayload;
    sendUdpSyslog(syslogPayload);
  }

  if ( receivedpayload == "{\"DOOR\":\"CHECKING\"}") {

    String check_doorpayload = "{\"DOOR\":";
    if ( doorStatus == 0 ) {
      check_doorpayload += "\"CHECK_CLOSED\"";
    }
    else {
      check_doorpayload += "\"CHECK_OPEN\"";
    }
    check_doorpayload += "}";
    sendmqttMsg(doortopic, check_doorpayload);
  }

}

void setup() {
  system_update_cpu_freq(SYS_CPU_80MHz);
  delay(20);

  rtc_check();


  pinMode(IRPIN, INPUT_PULLUP);
  pinMode(DOORPIN, INPUT_PULLUP);

  doorStatus = digitalRead(DOORPIN);
  olddoorStatus = doorStatus;

  attachInterrupt(4, IRCHECKING_START, RISING);
  attachInterrupt(5, DOORCHECKING, CHANGE);

  wifi_connect();

  lastReconnectAttempt = 0;
  revCounts = rtc_mem_revcount.salt ;

  getResetInfo = "hello from ESP8266 s07 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  startMills = sentMills = timemillis = millis();
  revMills  = revValue = oldrevMills = oldrevValue = 0 ;


  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);


  //OTA
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-power");
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

  //emon1.current(A0, 60.6);
  emon1.current(A0, 85);
  oldirStatus = LOW ;
}

void DOORCHECKING() {
  doorStatus = digitalRead(DOORPIN);
}

void ICACHE_RAM_ATTR IRCHECKING_START() {
  detachInterrupt(4);
  attachInterrupt(4, count_powermeter, RISING);
  startMills = millis();
  oldirStatus = HIGH ;
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
      if (now - lastReconnectAttempt > 200) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      if ((millis() - sentMills) > ( 700) ) {
        unsigned long emonmillis = millis();
        VIrms = emon1.calcIrms(1480) * 220.0;
        //VIrms = emon1.calcIrms(2960) * 220.0;
        calcIrmsmillis = millis() - emonmillis;

        ave.push(VIrms);
        average = ave.mean();
      }

      if ( revMills > 600 ) {
        revValue = (float(( 3600  * 1000 ) / ( 600 * float(revMills) ) ) * 1000);
      }

      if ( oldrevValue == 0 ) {
        oldrevValue = revValue;
      }

      if ( oldrevMills == 0 ) {
        oldrevMills = revMills;
      }

      payload = "{\"VIrms\":";
      payload += average;
      payload += ",\"revValue\":";
      payload += revValue;
      payload += ",\"revMills\":";
      payload += revMills;
      payload += ",\"powerAvg\":";
      payload += (( average + revValue ) / 2) ;
      payload += ",\"Stddev\":";
      payload += ave.stddev();
      payload += ",\"calcIrmsmillis\":";
      payload += calcIrmsmillis;
      payload += ",\"revCounts\":";
      payload += revCounts;
      payload += ",\"FreeHeap\":";
      payload += ESP.getFreeHeap();
      payload += ",\"RSSI\":";
      payload += WiFi.RSSI();
      payload += ",\"millis\":";
      payload += (millis() - timemillis);
      payload += "}";

      if ( doorStatus != olddoorStatus ) {

        doorpayload = "{\"DOOR\":";

        if ( doorStatus == 0 ) {
          doorpayload += "\"CLOSED\"";
        }
        else {
          doorpayload += "\"OPEN\"";
        }
        doorpayload += "}";

        sendmqttMsg(doortopic, doorpayload);
        olddoorStatus = doorStatus ;
      }

      if (((millis() - sentMills) > REPORT_INTERVAL ) && ( irStatus == oldirStatus )) {
        sendmqttMsg(topic, payload);
        sentMills = millis();
      }

      if ( irStatus != oldirStatus ) {
        rtc_mem_revcount.salt = revCounts;
        if (system_rtc_mem_write(100, &rtc_mem_revcount, sizeof(rtc_mem_revcount))) {
          if (DEBUG_PRINT) {
            //
          }
        } else {
          if (DEBUG_PRINT) {
            sendUdpSyslog("rtc mem write is fail");
          }
        }

        sendmqttMsg(topic, payload);

        sentMills = millis();
        oldirStatus = irStatus ;
        oldrevValue = revValue ;
        oldrevMills = revMills ;

      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}

void ICACHE_RAM_ATTR sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-power: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payload) {
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, 1)) {
    /*
      if (INFO_PRINT) {
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish ok";
      sendUdpSyslog(syslogPayload);
      }
    */
    free(p);
  } else {
    if (DEBUG_PRINT) {
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish fail";
      sendUdpSyslog(syslogPayload);
    }
    free(p);
  }
  client.loop();
}


void ICACHE_RAM_ATTR count_powermeter() {
  // 600 rev/kWh --> 1 rev 6 sec 1kW
  // max A : 40
  // 220 V * 45A = 9900,
  // 0.6sec -> 10kW
  //

  if (( millis() - startMills ) < 600 ) {
    return;
  }
  if (( millis() - startMills ) < ( revMills / 4 )) {
    return;
  } else {
    revMills   = (millis() - startMills)  ;
    startMills = millis();
    irStatus   = !irStatus ;
    revCounts++;
  }
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
