#include <Wire.h>
// https://github.com/Makuna/Rtc
#include <RtcDS1307.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "/usr/local/src/ap_setting.h"

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
  uint32_t pls;
  uint16_t ct1;
  uint16_t ct2;
  uint16_t ct3;
  uint16_t pad;
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
  Serial.swap();
  Wire.begin(0, 2);
  //twi_setClock(100000);

  wifi_connect();

  pinMode(DATA_IS_RDY_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN), data_isr, CHANGE);

}

void loop() {
  if (!data_is_rdy) {
    detachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN));
    pinMode(DATA_IS_RDY_PIN, OUTPUT);
    digitalWrite(DATA_IS_RDY_PIN, HIGH);
    delayMicroseconds(5);

    int start_address = 0;
    uint8_t* to_read_current = reinterpret_cast< uint8_t*>(&sensor_data);
    uint8_t gotten = Rtc.GetMemory(start_address, to_read_current, sizeof(sensor_data));

    delayMicroseconds(5);
    digitalWrite(DATA_IS_RDY_PIN, LOW);
    pinMode(DATA_IS_RDY_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN), data_isr, CHANGE);

    Serial.print("GetMemory result : ");
    Serial.println(gotten);

    syslogPayload  = "_salt : ";
    syslogPayload += sensor_data._salt;
    syslogPayload += " - pls : ";
    syslogPayload += sensor_data.pls;
    syslogPayload += " - ct1 : ";
    syslogPayload += sensor_data.ct1;
    syslogPayload += " - ct2 : ";
    syslogPayload += sensor_data.ct2;
    syslogPayload += " - ct3 : ";
    syslogPayload += sensor_data.ct3;
    syslogPayload += " - pad : ";
    syslogPayload += sensor_data.pad;
    syslogPayload += " == ";
    syslogPayload += sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3;

    sendUdpSyslog(syslogPayload);
  }
}
