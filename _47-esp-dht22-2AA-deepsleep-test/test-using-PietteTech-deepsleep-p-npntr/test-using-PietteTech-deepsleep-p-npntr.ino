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

// to check battery voltage using internal adc
ADC_MODE(ADC_VCC);

// rtc
#define RTC_MAGIC 12345

typedef struct _tagPoint {
  uint32 magic;
  uint32 salt;
  uint32 wifi_err_cnt;
  uint32 temp_err_cnt;
  uint32 report_cnt;
} RTC_TEST;

RTC_TEST rtc_mem_test;

//
#define IPSET_STATIC { 192, 168, 10, 17 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

const IPAddress ip_static = IPSET_STATIC;
const IPAddress ip_gateway = IPSET_GATEWAY;
const IPAddress ip_subnet = IPSET_SUBNET;
const IPAddress ip_dns = IPSET_DNS;

//
const IPAddress influxdbudp = MQTT_SERVER;

// wifi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

//
int vdd;
unsigned int localPort = 2390;

WiFiClient wifiClient;
WiFiUDP udp;

// system defines
#define DHTTYPE  DHT22              // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   2                  // Digital pin for communications
#define REPORT_INTERVAL 300          // in sec
#define DHT_SMAPLING_INTERVAL  2500 // in msec 
#define DHT_GND_PIN 0 // to control npn tr

unsigned long startMills, checkMillis;
bool bDHTstarted;
int acquirestatus;
int loopcount;

void ICACHE_RAM_ATTR dht_wrapper();

PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

void goingtosleep() {
  digitalWrite(DHT_GND_PIN, LOW);
  delay(100);
  system_rtc_mem_write(100, &rtc_mem_test, sizeof(rtc_mem_test));
  ESP.deepSleep((REPORT_INTERVAL * 1000 * 1000 ), WAKE_RF_DEFAULT);
  yield();
}

void dorestart() {
  digitalWrite(DHT_GND_PIN, LOW);
  delay(200);
  
  system_rtc_mem_write(100, &rtc_mem_test, sizeof(rtc_mem_test));
  ESP.restart();
  yield();
}

void rtc_check() {
  // system_rtc_mem_read(64... not work, use > 64
  system_rtc_mem_read(100, &rtc_mem_test, sizeof(rtc_mem_test));
  if (rtc_mem_test.magic != RTC_MAGIC) {
    rtc_mem_test.magic        = RTC_MAGIC;
    rtc_mem_test.salt         = 0;
    rtc_mem_test.wifi_err_cnt = 0;
    rtc_mem_test.temp_err_cnt = 0;
    rtc_mem_test.report_cnt   = 0;
  }
  rtc_mem_test.salt++;
  system_rtc_mem_write(100, &rtc_mem_test, sizeof(rtc_mem_test));
}

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
  delay(100);
}

void ICACHE_RAM_ATTR printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
  volatile uint8_t *_e = &_d->_edges[0];
  int result = _d->getStatus();
  if (result != 0) {
    rtc_mem_test.temp_err_cnt++;
  }
  rtc_mem_test.report_cnt++;

  String udppayload = "edges2,device=esp-12-N3,DHTLIB_ONE_TIMING=110 ";
  for (n = 0; n < 41; n++) {
    char buf[2];
    udppayload += "e";
    sprintf(buf, "%02d", n);
    udppayload += buf;
    udppayload += "=";
    udppayload += *_e++;
    udppayload += "i,";
  }

  udppayload += "F=";
  udppayload += ESP.getCpuFreqMHz();
  udppayload += "i,C=";
  udppayload += rtc_mem_test.salt;
  udppayload += "i,O=";
  udppayload += rtc_mem_test.report_cnt;
  udppayload += "i,R=";
  udppayload += result;
  udppayload += ",E=";
  udppayload += rtc_mem_test.temp_err_cnt;
  udppayload += "i,W=";
  udppayload += rtc_mem_test.wifi_err_cnt;
  udppayload += "i,H=";
  udppayload += _d->getHumidity();
  udppayload += ",T=";
  udppayload += _d->getCelsius();
  udppayload += ",vdd=";
  udppayload += vdd;
  udppayload += "i";

  Serial.println(udppayload);
  sendUdpmsg(udppayload);
}

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(ip_static, ip_gateway, ip_subnet, ip_dns);
  WiFi.hostname("esp-dht22-deepsleeptest");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      rtc_mem_test.wifi_err_cnt++;
      goingtosleep();
    }
  }
  /*
    Serial.println(millis() - startMills);

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  */
}

void setup() {

  Serial.begin(74880);
  Serial.println("");
  Serial.println("Setup started");


  rtc_check();

  vdd = ESP.getVcc() * 0.96;
  acquirestatus = loopcount = 0;

  pinMode(DHT_GND_PIN, OUTPUT);
  digitalWrite(DHT_GND_PIN, HIGH);

  Serial.println("wifi on");
  if (WiFi.status() != WL_CONNECTED) {
    wifi_connect();
  }
  Serial.println("wifi connected");
  bDHTstarted = false;
  startMills = checkMillis = millis();
}

void loop() {
  if ( loopcount < 2 ) {
    if (bDHTstarted) {
      acquirestatus = DHT.acquiring();
      if (!acquirestatus) {
        Serial.println("dht started");
        if (DHT.getStatus() != 0) {
          rtc_mem_test.temp_err_cnt++;
          printEdgeTiming(&DHT);
          dorestart();
          //goingtosleep();
        }

        bDHTstarted = false;
        loopcount++;
      }
    }

    if ((millis() - checkMillis) > DHT_SMAPLING_INTERVAL ) {
      checkMillis = millis();

      if (acquirestatus == 1) {
        rtc_mem_test.temp_err_cnt++;
        DHT.reset();
      }

      if (!bDHTstarted) {
        DHT.acquire();
        bDHTstarted = true;
        Serial.println("starting dht");
      }
    }
  } else {
    Serial.println("reporting -->");
    printEdgeTiming(&DHT);
    Serial.println("reporting done --> ");
    Serial.println("going to sleep --> ");
    goingtosleep();
  }

  if ((millis() - startMills) > 25000) {
    Serial.println("timed out");
    goingtosleep();
  }
}
