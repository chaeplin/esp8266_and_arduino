// 80MHz, 1M / 64K SPIFFS
#include <Wire.h>
// https://github.com/Makuna/Rtc
#include <RtcDS1307.h>
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

/*
  atmega328  -  esp - ds1307
  3v
  gnd
  scl - d2  - scl
  sda - d0  - sda // led
  d2 -      - sqw
  d6  - tx
  d7  - rst
  d12 -  -        // button to inform flashing of esp
*/

#define DATA_IS_RDY_PIN 1

typedef struct
{
  uint32_t _salt;
  uint32_t pls_no;
  uint16_t pls_wh;
  uint16_t ct1_wh;
  uint16_t ct2_wh;
  uint16_t ct3_wh;
  uint16_t ct1_vr;
  uint8_t  door;
  uint8_t  pad1;
} data;

data sensor_data;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

volatile bool data_is_rdy;
String syslogPayload;

RtcDS1307 Rtc;
WiFiClient wifiClient;
WiFiUDP udp;

void data_isr() {
  data_is_rdy = digitalRead(DATA_IS_RDY_PIN);
}

void sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-emontxv2: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void wifi_connect() {
  //wifi_set_phy_mode(PHY_MODE_11N);
  //system_phy_set_max_tpw(1);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-emontxv2");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      ESP.restart();
    }
  }
}

void setup() {
  system_update_cpu_freq(SYS_CPU_80MHz);
  Serial.swap();
  Wire.begin(0, 2);
  //twi_setClock(100000);

  data_is_rdy = false;

  wifi_connect();

  pinMode(DATA_IS_RDY_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN), data_isr, RISING);

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

}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (data_is_rdy) {
      detachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN));
      delayMicroseconds(5);
      pinMode(DATA_IS_RDY_PIN, OUTPUT);
      digitalWrite(DATA_IS_RDY_PIN, LOW);
      delayMicroseconds(5);

      int start_address = 0;
      uint8_t* to_read_current = reinterpret_cast< uint8_t*>(&sensor_data);
      uint8_t gotten = Rtc.GetMemory(start_address, to_read_current, sizeof(sensor_data));

      delayMicroseconds(5);
      digitalWrite(DATA_IS_RDY_PIN, HIGH);
      pinMode(DATA_IS_RDY_PIN, INPUT_PULLUP);
      delayMicroseconds(5);
      attachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN), data_isr, RISING);

      Serial.print("GetMemory result : ");
      Serial.println(gotten);

      syslogPayload  = "_salt : ";
      syslogPayload += sensor_data._salt;
      syslogPayload += " - pls_no : ";
      syslogPayload += sensor_data.pls_no;
      syslogPayload += " - pls_wh : ";
      syslogPayload += sensor_data.pls_wh;
      syslogPayload += " - ct1 : ";
      syslogPayload += sensor_data.ct1_wh;
      syslogPayload += " - ct2 : ";
      syslogPayload += sensor_data.ct2_wh;
      syslogPayload += " - ct3 : ";
      syslogPayload += sensor_data.ct3_wh;
      syslogPayload += " - ct1 vr : ";
      syslogPayload += sensor_data.ct1_vr;
      syslogPayload += " - door : ";
      syslogPayload += sensor_data.door;
      syslogPayload += " - pad1 : ";
      syslogPayload += sensor_data.pad1;
      syslogPayload += " == ";
      syslogPayload += uint8_t(sensor_data.pls_wh + sensor_data.ct1_wh + sensor_data.ct2_wh + sensor_data.ct3_wh + sensor_data.door);

      sendUdpSyslog(syslogPayload);

      data_is_rdy = false;
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}
