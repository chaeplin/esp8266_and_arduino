// 160MHz, 1M / 64K SPIFFS / esp-emontxv2
#include <Wire.h>
// https://github.com/Makuna/Rtc
#include <RtcDS1307.h>
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Average.h>
#include "/usr/local/src/ap_setting.h"

extern "C"
{
#include "user_interface.h"
}

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

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

#define DEBUG_PRINT 1
#define DATA_IS_RDY_PIN 1

// rtc size --> 56 byte
static const uint8_t RTC_READ_ADDRESS  = 0;

//
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length)
{
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

//
template <class T> uint32_t calc_hash(T& data) {
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

struct
{
  uint32_t hash;
  uint32_t pls_no;
  uint16_t pls_ts;
  int16_t ct1_rp;
  int16_t ct1_ap;
  int16_t ct1_vr;
  uint16_t ct1_ir;
  int16_t ct2_rp;
  int16_t ct3_rp;
  uint16_t door;
} sensor_data;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

// mqtt
char* topic       = "esp8266/arduino/s07";
char* doortopic   = "esp8266/arduino/s05" ;
char* hellotopic  = "HELLO";
char* willTopic   = "clients/power";
char* willMessage = "0";
char* subtopic    = "esp8266/check";

//
volatile bool bdata_is_rdy;
bool ok;
uint32_t _hash;
int16_t pls_p = 0;
bool bdoor_status;
bool ResetInfo = false;
long lastReconnectAttempt = 0;

String syslogPayload, clientName, payload, doorpayload, getResetInfo;

//
void callback(char* intopic, byte* inpayload, unsigned int length);

//
RtcDS1307 Rtc;
WiFiClient wifiClient;
WiFiUDP udp;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
Average<float> ave1(3);

void data_isr()
{
  bdata_is_rdy = digitalRead(DATA_IS_RDY_PIN);
}

void ICACHE_RAM_ATTR sendUdpmsg(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
  delay(100);
}

void ICACHE_RAM_ATTR sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-emontxv2: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payload)
{
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, 1))
  {
    /*
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish ok";
      sendUdpSyslog(syslogPayload);
    */
    free(p);
  }
  else
  {
    syslogPayload = topictosend;
    syslogPayload += " - ";
    syslogPayload += payload;
    syslogPayload += " : Publish fail";
    sendUdpSyslog(syslogPayload);
    free(p);
  }
  client.loop();
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++)
  {
    receivedpayload += (char)inpayload[i];
  }

  syslogPayload = intopic;
  syslogPayload += " ====> ";
  syslogPayload += receivedpayload;
  sendUdpSyslog(syslogPayload);

  if ( receivedpayload == "{\"CHECKING\":\"1\"}")
  {
    /*
    String check_doorpayload = "{\"DOOR\":";
    if ( sensor_data.door == 0 ) {
      check_doorpayload += "\"CHECK_CLOSED\"";
    }
    else
    {
      check_doorpayload += "\"CHECK_OPEN\"";
    }
    check_doorpayload += "}";
    */
    String check_doorpayload = "door: ";
    if ( sensor_data.door == 0 ) {
      check_doorpayload += "check_closed";
    }
    else
    {
      check_doorpayload += "check_open";
    }
    check_doorpayload += "\r\n";
    check_doorpayload += "powerAvg: ";
    check_doorpayload += sensor_data.ct1_rp;
    check_doorpayload += "\r\n";
    check_doorpayload += "powerAC: ";
    check_doorpayload += ave1.mean();
    
    sendmqttMsg(doortopic, check_doorpayload);
  }
}

boolean reconnect() {
  if (!client.connected())
  {
    if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage))
    {
      client.publish(willTopic, "1", true);
      if ( ResetInfo == false)
      {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        ResetInfo = true;
      }
      else
      {
        client.publish(hellotopic, "hello again 1 from esp-power");
      }
      client.subscribe(subtopic);
      sendUdpSyslog("---> mqttconnected");
    }
    else
    {
      syslogPayload = "failed, rc=";
      syslogPayload += client.state();
      sendUdpSyslog(syslogPayload);
    }
  }
  client.loop();
  return client.connected();
}

void wifi_connect()
{
  //wifi_set_phy_mode(PHY_MODE_11N);
  //system_phy_set_max_tpw(1);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-emontxv2");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      ESP.restart();
    }
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

void ICACHE_RAM_ATTR send_raw_data()
{


  syslogPayload = "pls_no : ";
  syslogPayload += sensor_data.pls_no;
  syslogPayload += " - pls_ts : ";
  syslogPayload += sensor_data.pls_ts;
  syslogPayload += " - pls_p : ";
  syslogPayload += pls_p;
  syslogPayload += " - ct1_rp : ";
  syslogPayload += sensor_data.ct1_rp;
  syslogPayload += " - ct1_ap : ";
  syslogPayload += sensor_data.ct1_ap;
  syslogPayload += " - ct1_vr : ";
  syslogPayload += sensor_data.ct1_vr;
  syslogPayload += " - ct1_ir : ";
  syslogPayload += sensor_data.ct1_ir;
  syslogPayload += " - ct2_rp : ";
  syslogPayload += sensor_data.ct2_rp;
  syslogPayload += " - ct3_rp : ";
  syslogPayload += sensor_data.ct3_rp;
  syslogPayload += " - door : ";
  syslogPayload += sensor_data.door;

  _hash = calc_hash(sensor_data);
  if (_hash != sensor_data.hash)
  {
    syslogPayload += " - hash : ";
    syslogPayload += sensor_data.hash;
    syslogPayload += " != ";
    syslogPayload += _hash;
  }

  sendUdpSyslog(syslogPayload);
}

void ICACHE_RAM_ATTR sendtoInfluxdb()
{
  String udppayload = "emontxv2,device=esp-01 ";
  udppayload += "F=";
  udppayload += ESP.getCpuFreqMHz();
  udppayload += "i,pls_no=";
  udppayload += sensor_data.pls_no;
  udppayload += "i,pls_ts=";
  udppayload += sensor_data.pls_ts;
  udppayload += "i,pls_p=";
  udppayload += pls_p;
  udppayload += "i,ct1_rp=";
  udppayload += sensor_data.ct1_rp;
  udppayload += "i,ct1_ap=";
  udppayload += sensor_data.ct1_ap;
  udppayload += "i,ct1_vr=";
  udppayload += sensor_data.ct1_vr;
  udppayload += "i,ct1_ir=";
  udppayload += sensor_data.ct1_ir;
  udppayload += "i,ct2_rp=";
  udppayload += sensor_data.ct2_rp;
  udppayload += "i,ct3_rp=";
  udppayload += sensor_data.ct3_rp;
  udppayload += "i,door=";
  udppayload += sensor_data.door;
  udppayload += "i";

  sendUdpmsg(udppayload);
}

bool ICACHE_RAM_ATTR get_i2c_data()
{
  if (digitalRead(DATA_IS_RDY_PIN))
  {
    pinMode(DATA_IS_RDY_PIN, OUTPUT);
    digitalWrite(DATA_IS_RDY_PIN, LOW);
    delayMicroseconds(5);

    uint8_t* to_read_current = reinterpret_cast< uint8_t*>(&sensor_data);
    uint8_t gotten = Rtc.GetMemory(RTC_READ_ADDRESS, to_read_current, sizeof(sensor_data));

    delayMicroseconds(5);
    digitalWrite(DATA_IS_RDY_PIN, HIGH);
    pinMode(DATA_IS_RDY_PIN, INPUT_PULLUP);
    return true;
  }
  else
  {
    return false;
  }
}

void setup()
{
  system_update_cpu_freq(SYS_CPU_160MHz);
  Serial.swap();
  //
  pinMode(DATA_IS_RDY_PIN, INPUT_PULLUP);
  //
  Wire.begin(0, 2);
  twi_setClock(200000);
  //
  wifi_connect();
  lastReconnectAttempt = 0;

  //
  getResetInfo = "hello from esp-emontxv2 ";
  getResetInfo += ESP.getResetInfo().substring(0, 50);

  //
  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  //OTA
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-emontxv2");
  ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]()
  {
    sendUdpSyslog("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]()
  {
    sendUdpSyslog("ArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    syslogPayload = "Progress: ";
    syslogPayload += (progress / (total / 100));
    sendUdpSyslog(syslogPayload);
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

  syslogPayload = "=====> unit started : pin 1 status : ";
  syslogPayload += digitalRead(1);
  sendUdpSyslog(syslogPayload);

  bdata_is_rdy = false;
  attachInterrupt(digitalPinToInterrupt(DATA_IS_RDY_PIN), data_isr, RISING);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED)
  {
    if (bdata_is_rdy)
    {
      while (!digitalRead(DATA_IS_RDY_PIN))
      {
        delay(5);
      }
      ok = get_i2c_data();
      if (sensor_data.pls_ts > 1)
      {
        pls_p = (float(( 3600  * 1000 ) / ( 600 * float(sensor_data.pls_ts) ) ) * 1000);
      }
      else
      {
        pls_p = 0;
      }
      ave1.push(sensor_data.ct3_rp);
      //send_raw_data();
      sendtoInfluxdb();
      bdata_is_rdy = false;
    }
    if (!client.connected())
    {
      syslogPayload = "failed, rc= ";
      syslogPayload += client.state();
      sendUdpSyslog(syslogPayload);

      unsigned long now = millis();
      if (now - lastReconnectAttempt > 200)
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
      if (bdoor_status != sensor_data.door)
      {
        doorpayload = "{\"DOOR\":";
        if ( sensor_data.door == 0 )
        {
          doorpayload += "\"CLOSED\"";
        }
        else
        {
          doorpayload += "\"OPEN\"";
        }
        doorpayload += "}";
        sendmqttMsg(doortopic, doorpayload);
        bdoor_status = sensor_data.door;
      }

      if (_hash != sensor_data.hash)
      {
        payload = "{\"powerAvg\":";
        payload += sensor_data.ct1_rp;
        payload += ",\"powerAC\":";
        if (ave1.mean() > 150)
        {
          payload += "1";
        }
        else
        {
          payload += "0";
        }
        payload += ",\"FreeHeap\":";
        payload += ESP.getFreeHeap();
        payload += ",\"RSSI\":";
        payload += WiFi.RSSI();
        payload += ",\"millis\":";
        payload += millis();
        payload += "}";

        sendmqttMsg(topic, payload);
        _hash = sensor_data.hash;
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
//
