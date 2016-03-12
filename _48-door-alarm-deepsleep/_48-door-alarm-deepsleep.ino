// CPU : 80MHz, FLASH : 4M/1M
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
// AP ssid/passwd
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "gpio.h"
#include "user_interface.h"
}

#define RST_PIN 16
#define DOOR_PIN 4
#define DOOR_INT_PIN 5

// for battery check
ADC_MODE(ADC_VCC);

#define INFO_PRINT 1

// static ip for fast wifi conn
#define IPSET_STATIC  { 192, 168, 10, 27 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET  { 255, 255, 255, 0 }
#define IPSET_DNS     { 192, 168, 10, 10 }
#define MQTT_SERVER   { 192, 168, 10, 10 }

IPAddress ip_static   = IPSET_STATIC;
IPAddress ip_gateway  = IPSET_GATEWAY;
IPAddress ip_subnet   = IPSET_SUBNET;
IPAddress ip_dns      = IPSET_DNS;
IPAddress mqtt_server = MQTT_SERVER;

// wifi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

char* topic = "sensor/door";

String clientName, payload;

unsigned long startMills;
unsigned int vbatt;
bool door_closed;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, wifiClient);

void goingToSleep() {
  yield();
  Serial.println("going to deepsleep");
  delay(100);
  ESP.deepSleep(0);
  yield();
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

void wifi_connect() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFiClient::setLocalPortStart(micros() + vbatt);
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.config(ip_static, ip_gateway, ip_subnet, ip_dns);
    WiFi.hostname("esp-door-sensor");

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Attempt++;
      if (Attempt == 150) {
        Serial.println("wifi conn fail");
        break;
      }
    }
    Serial.println("wifi connected");
  }
}

boolean mqtt_publish(char* topic_tosend, String payload_tosend) {
  if (WiFi.status() == WL_CONNECTED) {
    if (client.connect((char*) clientName.c_str())) {
      unsigned int msg_length = payload_tosend.length();
      byte* p = (byte*)malloc(msg_length);
      memcpy(p, (char*) payload_tosend.c_str(), msg_length);

      if (client.publish(topic_tosend, p, msg_length, 1)) {
        free(p);
        client.disconnect();
        Serial.println(payload_tosend);
        return 1;
      } else {
        free(p);
        client.disconnect();
        return 0;
      }
    } else {
      Serial.println("mqtt conn failed");
      return 0;
    }
  } else {
    Serial.println("wifi not connected");
    wifi_connect();
    return 0;
  }
}

boolean sendMsg(char* topic_tosend, String payload_tosend) {
  int Attempt = 0;
  while (mqtt_publish(topic, payload) == 0) {
    delay(100);
    Attempt++;
    if (Attempt == 5) {
      Serial.println("mqtt pub failed");
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("");

  startMills = millis();
  vbatt = ESP.getVcc() * 0.96;
  // to cut out reset line, normaly don't need
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);

  // input of door switch, closed : 1, open : 0
  pinMode(DOOR_PIN, INPUT);

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  WiFi.setAutoConnect(true);
  wifi_connect();

  door_closed = digitalRead(DOOR_PIN);

  payload = "{\"vbatt\":";
  payload += vbatt;
  payload += ",\"millis\":";
  payload += (millis() - startMills);
  payload += ",\"door_closed\":";
  payload += door_closed;
  payload += "}";

  sendMsg(topic, payload);

  // if door_closed = 0 : door is opened, or power on while door is opened.
  // ==> wait 10 sec or INT, alarm door is opened too long, wait till door is closed. Because don't have int or reset when door is closed.
  // if door_closed = 1 : door is closed, or power on while door is closed.
  // ==> going to deep sleep.
  //

  // door is closed
  if (door_closed == true) {
    goingToSleep();
  }
}

void loop() {
  // modem sleep
  Serial.println("going to modem sleep / 10sec");
  client.disconnect();
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);
  wifi_set_sleep_type(MODEM_SLEEP_T);
  wifi_fpm_open();
  wifi_fpm_do_sleep(0xFFFFFFF);

  // check door status
  int cnt = 200;
  while ( cnt != 0 ) {
    if (door_closed != digitalRead(DOOR_PIN)) {
      break;
    }
    delay(50);
    cnt--;
  }

  // wake up to use WiFi again
  wifi_fpm_do_wakeup();
  wifi_fpm_close();

  wifi_set_opmode(STATION_MODE);
  wifi_station_connect();
  wifi_connect();

  door_closed = digitalRead(DOOR_PIN);
  // door is closed
  if (door_closed == true) {
    payload = "{\"vbatt\":";
    payload += vbatt;
    payload += ",\"millis\":";
    payload += (millis() - startMills);
    payload += ",\"door_closed\":";
    payload += door_closed;
    payload += "}";

    sendMsg(topic, payload);
    goingToSleep();
  } else {
    payload = "{\"vbatt\":";
    payload += vbatt;
    payload += ",\"millis\":";
    payload += (millis() - startMills);
    payload += ",\"door_closed\":";
    payload += door_closed;
    payload += ",\"too_long\":";
    payload += true;
    payload += "}";

    sendMsg(topic, payload);
  }

  Serial.println("going to light sleep / 0xFFFFFFF");
  delay(100);
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);
  wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  gpio_pin_wakeup_enable(GPIO_ID_PIN(DOOR_INT_PIN), GPIO_PIN_INTR_HILEVEL);
  wifi_fpm_do_sleep(0xFFFFFFF);
  delay(200);

  gpio_pin_wakeup_disable();
  Serial.println("wake up / 0xFFFFFFF");
  wifi_fpm_close();
  wifi_set_opmode(STATION_MODE);
  wifi_station_connect();
  wifi_connect();
  delay(100);
  door_closed = digitalRead(DOOR_PIN);

  payload = "{\"vbatt\":";
  payload += vbatt;
  payload += ",\"millis\":";
  payload += (millis() - startMills);
  payload += ",\"door_closed\":";
  payload += door_closed;
  payload += "}";

  sendMsg(topic, payload);
  delay(100);
  goingToSleep();

}
//
