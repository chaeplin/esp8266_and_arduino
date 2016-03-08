// CPU 80MHZ, FLASH 4M/1M
/*
  *** Sample using esp-01, D16 is connected to RST
  *** If DHT22 is powered by a gpio( VCC of DHT22 is connected to a gpio) and OUTPUT of DHT22 is connected to D2, boot will fail.
  *** Power off ----> D2 is in LOW( == DHT22 is in LOW) ==> SDIO boot mode.


  Temperature and humidity values are each read out the results of the last measurement.
  For real-time data that need continuous read twice, we recommend repeatedly to read sensors,
  and each read sensor interval is greater than 2 seconds to obtain accuratethe data.
  --> read twice !!!!
  --> sampling period : 2 sec



  AM2302 have to wait for the power (on AM2302 power 2S crossed the unstable state,
  the device can not send any instructions to read during this period),
  the test environment temperature and humidity data, and record data,
  since the sensor into a sleep state automatically.
  --> need 2 sec after power on

  3.3V - 5.5V is needed, min is 3.3V
  So if DHT22 is powered by battery(2 * AA), boost module is needed.
  http://www.ebay.com/itm/2-in-1-DC-DC-Step-Down-Step-Up-Converter-1-8V-5V-3V-3-7V-to-3-3V-Power-Module-/271873956073

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "PietteTech_DHT.h"
// ap setting
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

#define DHT_DEBUG_TIMING

// to check battery voltage using internal adc
ADC_MODE(ADC_VCC);

//
IPAddress influxdbudp = MQTT_SERVER;

// wifi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

//
int vdd;
unsigned int localPort = 2390;

WiFiClient wifiClient;
WiFiUDP udp;

// system defines
#define DHTTYPE  DHT22          // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   2              // Digital pin for communications
#define REPORT_INTERVAL 5000 // in msec

// to check dht
unsigned long startMills, sentMills;
bool bDHTstarted;       // flag to indicate we started acquisition
float t, h;
int acquireresult;
int acquirestatus;
int _sensor_error_count;
unsigned long _sensor_report_count;

//declaration
void ICACHE_RAM_ATTR dht_wrapper(); // must be declared before the lib initialization

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// This wrapper is in charge of calling
// must be defined like this for the lib work
void ICACHE_RAM_ATTR dht_wrapper() {
  DHT.isrCallback();
}

void ICACHE_RAM_ATTR sendUdpmsg(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
#if defined(DHT_DEBUG_TIMING)
  volatile uint8_t *_e = &_d->_edges[0];
#endif
  int result = _d->getStatus();
  if (result != 0) {
    _sensor_error_count++;
  }
  _sensor_report_count++;

  String udppayload = "edges2,device=esp-12-N3,debug=on,DHTLIB_ONE_TIMING=110 ";
  for (n = 0; n < 41; n++) {
    char buf[2];
    udppayload += "e";
    sprintf(buf, "%02d", n);
    udppayload += buf;
    udppayload += "=";
#if defined(DHT_DEBUG_TIMING)
    udppayload += *_e++;
#endif
    udppayload += "i,";
  }

  udppayload += "F=";
  udppayload += ESP.getCpuFreqMHz();
  udppayload += "i,C=";
  udppayload += _sensor_report_count;
  udppayload += "i,R=";
  udppayload += result;
  udppayload += ",E=";
  udppayload += _sensor_error_count;
  udppayload += "i,H=";
  udppayload += _d->getHumidity();
  udppayload += ",T=";
  udppayload += _d->getCelsius();
  udppayload += ",vdd=";
  udppayload += vdd;
  udppayload += "i";

  sendUdpmsg(udppayload);
}

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-dht22-deepsleeptest");

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

void setup()
{
  startMills = sentMills = millis();

  vdd = ESP.getVcc() * 0.96;

  _sensor_error_count = _sensor_report_count = acquirestatus = 0;

  delay(2000);
  acquireresult = DHT.acquireAndWait(500);

  if (acquireresult != 0) {
    h = t = 0;
  }
  wifi_connect();
}

void loop()
{

  if (WiFi.status() == WL_CONNECTED) {

    if (bDHTstarted) {
      acquirestatus = DHT.acquiring();
      if (!acquirestatus) {
        acquireresult = DHT.getStatus();
#if defined(DHT_DEBUG_TIMING)
        printEdgeTiming(&DHT);
#endif
        if ( acquireresult == 0 ) {
          t = DHT.getCelsius();
          h = DHT.getHumidity();
        }
        bDHTstarted = false;
      }
    }

    if ((millis() - sentMills) > REPORT_INTERVAL ) {
#if defined(DHT_DEBUG_TIMING)
      printEdgeTiming(&DHT);
#endif
      sentMills = millis();

      if (acquirestatus == 1) {
        DHT.reset();
      }

      if (!bDHTstarted) {
        DHT.acquire();
        bDHTstarted = true;
      }
    }

  } else {
    wifi_connect();
  }
}
