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

#define DEBUG_PRINT 1

#define REPORT_INTERVAL 10  // in sec
#define SAMPLING_INTERVAL 2 // in sec

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

// to check battery voltage using internal adc
ADC_MODE(ADC_VCC);

// static ip
#define IPSET_STATIC { 192, 168, 10, 21 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

IPAddress ip_static   = IPSET_STATIC;
IPAddress ip_gateway  = IPSET_GATEWAY;
IPAddress ip_subnet   = IPSET_SUBNET;
IPAddress ip_dns      = IPSET_DNS;
IPAddress influxdbudp = MQTT_SERVER;

// wifi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// udp
unsigned int localPort = 2390;

//
int vdd;
unsigned long startMills;

// rtc
#define RTC_MAGIC 12345678

typedef struct _tagPoint {
  uint32 magic;
  uint32 salt;
  uint32 wifi_err_cnt;
  uint32 temp_err_cnt;
  uint32 report_cnt;
  bool report_next;
} RTC_TEST;

RTC_TEST rtc_mem_test;

//
// system defines
#define DHTTYPE  DHT22        // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   2            // Digital pin for communications

//declaration
void ICACHE_RAM_ATTR dht_wrapper(); // must be declared before the lib initialization

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
int acquireresult;
int acquirestatus;
float h, t;

// This wrapper is in charge of calling
// must be defined like this for the lib work
void ICACHE_RAM_ATTR dht_wrapper() {
  DHT.isrCallback();
}

//
WiFiClient wifiClient;
WiFiUDP udp;

void goingToSleep()
{
  if (DEBUG_PRINT) {
    Serial.println(" -----> goingToSleep");
    delay(100);
  }
  system_rtc_mem_write(100, &rtc_mem_test, sizeof(rtc_mem_test));
  system_deep_sleep_set_option(2);
  if (rtc_mem_test.report_next) {
    ESP.deepSleep((SAMPLING_INTERVAL * 1000 * 1000 ), WAKE_RF_DEFAULT);
  } else {
    ESP.deepSleep((REPORT_INTERVAL * 1000 * 1000 ), WAKE_RF_DISABLED);
  }
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
    rtc_mem_test.report_next  = false;
  }
  rtc_mem_test.salt++;
}

void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-dht22-deepsleeptest");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    if (DEBUG_PRINT) {
      Serial.print(". ");
      Serial.print(Attempt);
    }
    delay(100);
    Attempt++;
    if (Attempt == 200)
    {
      // if fail
      rtc_mem_test.wifi_err_cnt++;
      rtc_mem_test.report_next = true;
      goingToSleep();
    }
  }
}

void sendUdpmsg(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
#if defined(DHT_DEBUG_TIMING)
  volatile uint8_t *_e = &_d->_edges[0];
#endif

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

  udppayload += "Freq=";
  udppayload += ESP.getCpuFreqMHz();
  udppayload += "i,Rcnt=";
  udppayload += rtc_mem_test.report_cnt;
  udppayload += "i,Tecnt=";
  udppayload += rtc_mem_test.temp_err_cnt;
  udppayload += "i,Wecnt=";
  udppayload += rtc_mem_test.wifi_err_cnt;
  udppayload += "i,H=";
  udppayload += _d->getHumidity();
  udppayload += ",T=";
  udppayload += _d->getCelsius();
  udppayload += ",vdd=";
  udppayload += vdd;
  udppayload += "i";

  if (DEBUG_PRINT) {
    Serial.println(udppayload);
    delay(100);
  }

  sendUdpmsg(udppayload);
}

void setup() {
  if (DEBUG_PRINT) {
    Serial.begin(115200);
    delay(10);
    Serial.println("");
  }

  system_update_cpu_freq(SYS_CPU_80MHz);

  startMills = millis();

  rtc_check();

  vdd = ESP.getVcc();

  WiFi.setAutoConnect(true);

  // power on, dht22 need 2 sec delay
  //if (rtc_mem_test.salt == 1) {
  delay(2000);
  //}

  acquireresult = DHT.acquireAndWait(100);

  if (DEBUG_PRINT) {
    Serial.print("acquireresult ---> : ");
    Serial.println(acquireresult);
  }

  if (acquireresult == 0) {
    t = DHT.getCelsius();
    h = DHT.getHumidity();
  } else {
    rtc_mem_test.temp_err_cnt++;
    rtc_mem_test.report_next = false;
    goingToSleep();
  }

  if (rtc_mem_test.report_next) {
    wifi_connect();

    if (WiFi.status() == WL_CONNECTED) {
      if (DEBUG_PRINT) {
        Serial.println("");
        Serial.println("reporting using udp ---> : ");
        delay(100);
      }

      // report
#if defined(DHT_DEBUG_TIMING)
      printEdgeTiming(&DHT);
#endif

      rtc_mem_test.report_cnt++;
      rtc_mem_test.report_next = false;
      goingToSleep();
    }
  } else {
    rtc_mem_test.report_next = true;
    goingToSleep();
  }

}

void loop() {
  // if err ocured
  rtc_mem_test.wifi_err_cnt++;
  rtc_mem_test.report_next = false;
  goingToSleep();
}
