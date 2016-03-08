#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "DHT.h"
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
#define REPORT_INTERVAL 300     // in sec

DHT dht(DHTPIN, DHTTYPE);

// to check dht
unsigned long startMills;
float t, h;
int acquireresult;
int _sensor_error_count;
unsigned long _sensor_report_count;

void sendUdpmsg(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void report()
{
  _sensor_report_count++;

  String udppayload = "edges2,device=esp-12-N3,debug=on,DHTLIB_ONE_TIMING=110 ";
  udppayload += "Freq=";
  udppayload += ESP.getCpuFreqMHz();
  udppayload += "i,C=";
  udppayload += _sensor_report_count;
  udppayload += "i,R=";
  udppayload += acquireresult;
  udppayload += ",E=";
  udppayload += _sensor_error_count;
  udppayload += "i,H=";
  udppayload += h;
  udppayload += ",T=";
  udppayload += t;
  udppayload += ",vdd=";
  udppayload += vdd;
  udppayload += "i";

  sendUdpmsg(udppayload);
}

void goingToSleep()
{
  Serial.println(" -----> goingToSleep");
  delay(100);
  ESP.deepSleep((REPORT_INTERVAL * 1000 * 1000 ));
  delay(250);
}


void wifi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-dht22-deepsleeptest");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(". ");
    Serial.print(Attempt);
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      // if fail
      ESP.restart();;
    }
  }
}

void setup()
{
  startMills = millis();
  Serial.begin(115200);
  Serial.println("");

  vdd = ESP.getVcc() * 0.96;
  Serial.println(vdd);
  //vdd = 0;

  dht.begin();

  delay(2000);

  h = dht.readHumidity();
  t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    acquireresult = -1;
    h = t = 0;
    Serial.println("Failed to read from DHT sensor!");
  } else {
    acquireresult = 0;
  }

  Serial.println("wifi on");
  wifi_connect();
  Serial.println("wifi conn done");
}

void loop()
{
  delay(5000);

  h = dht.readHumidity();
  t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    _sensor_error_count++;
    acquireresult = -1;
    h = t = 0;
    Serial.println("Failed to read from DHT sensor!");
  } else {
    acquireresult = 0;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("reporting using udp ---> : ");
    report();
  } else {
    wifi_connect();
  }
}
