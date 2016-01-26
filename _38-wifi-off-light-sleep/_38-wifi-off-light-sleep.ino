#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

#define FPM_SLEEP_MAX_TIME 0xFFFFFFF

#define IPSET_STATIC { 192, 168, 10, 7 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

IPAddress ip_static = IPSET_STATIC;
IPAddress ip_gateway = IPSET_GATEWAY;
IPAddress ip_subnet = IPSET_SUBNET;
IPAddress ip_dns = IPSET_DNS;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress mqtt_server = MQTT_SERVER;

char* topic = "pubtest";

String clientName;
long lastReconnectAttempt = 0;
long lastMsg = 0;
int test_para = 1;
unsigned long startMills;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, wifiClient);

void sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.print(payload);

    unsigned int msg_length = payload.length();

    Serial.print(" length: ");
    Serial.println(msg_length);

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length)) {
      Serial.println("Publish ok");
      free(p);
      //return 1;
    } else {
      Serial.println("Publish failed");
      free(p);
      //return 0;
    }
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

boolean reconnect()
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("===> mqtt connected");
    } else {
      Serial.print("---> mqtt failed, rc=");
      Serial.println(client.state());
    }
  }
  return client.connected();
}

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED) {
    // WIFI
    Serial.println();
    Serial.print("===> WIFI ---> Connecting to ");
    Serial.println(ssid);
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(". ");
      Serial.print(Attempt);
      delay(100);
      Attempt++;
      if (Attempt == 250)
      {
        Serial.println();
        Serial.println("-----> Could not connect to WIFI");
        ESP.restart();
        delay(200);
      }

    }
    Serial.println();
    Serial.print("===> WiFi connected");
    Serial.print(" ------> IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup()
{
  startMills = millis();
  Serial.begin(74880);

  Serial.println("");
  Serial.println("rtc mem test");
  Serial.println(wifi_station_get_auto_connect());
  WiFi.setAutoConnect(true);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

}

void fpm_wakup_cb_func1()
{
  Serial.println("Reconnecting");
  wifi_fpm_close();
  wifi_set_opmode(STATION_MODE);
  wifi_station_connect();
  wifi_connect();
}

void loop()
{
  Serial.println("starting main loop");
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 200) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    }
  } else {
    //wifi_connect();
  }

  if (client.connected()) {
    String payload = "{\"startMills\":";
    payload += (millis() - startMills);
    payload += ",\"FreeHeap\":";
    payload += ESP.getFreeHeap();
    payload += ",\"RSSI\":";
    payload += WiFi.RSSI();
    payload += "}";
    sendmqttMsg(topic, payload);
  }

  Serial.println("diconnecting client and wifi");
  client.disconnect();
  wifi_station_disconnect();
  wifi_set_opmode(NULL_MODE);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  wifi_fpm_open();
  // ---> not yet includef in user_interface.h, so can't use it now
  // 'wifi_fpm_set_wakeup_cb' was not declared in this scope
  wifi_fpm_set_wakeup_cb(fpm_wakup_cb_func1);
  wifi_fpm_do_sleep(100000 * 1000);
}

