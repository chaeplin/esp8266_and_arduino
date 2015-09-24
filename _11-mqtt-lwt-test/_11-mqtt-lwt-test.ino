#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#define _IS_MY_HOME 1
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif


char* topic   = "esp8266/arduino/s20";
char* hellotopic = "HELLO";

char* willTopic = "clients/s20";
char* willMessage = "0";

String clientName;
String payload;
WiFiClient wifiClient;

IPAddress server(192, 168, 10, 10);
PubSubClient client(server, 1883, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

unsigned long startMills;

long lastReconnectAttempt = 0;

void wifi_connect() {
  // WIFI
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    Serial.print(".");
    if (Attempt == 100)
    {
      Serial.println();
      Serial.println("Could not connect to WIFI");
      ESP.restart();
      delay(2000);
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //startMills = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(willTopic, "1", true);
        client.publish(hellotopic, "hello again wifi and mqtt from ESP8266 s20");
        Serial.println("reconnecting wifi and mqtt");
      } else {
        Serial.println("mqtt publish fail after wifi reconnect");
      }
    } else {
      client.publish(hellotopic, "hello again wifi from ESP8266 s20");
    }
  }

}

boolean reconnect() {
  if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
    Serial.println("connected");
    client.publish(hellotopic, "hello again 1 from ESP8266 s20");
    client.publish(willTopic, "1", true);
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
  }
  //startMills = millis();
  return client.connected();
}

void setup() {
  Serial.begin(38400);
  Serial.println("lwt test");
  Serial.println("ESP.getFlashChipSize() : ");
  Serial.println(ESP.getFlashChipSize());
  delay(100);

  startMills = millis();

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;

  String getResetInfo = "hello from ESP8266 s20 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        client.publish(willTopic, "1", true);
        Serial.print("Sending payload: ");
        Serial.println(getResetInfo);
      }
    } else {
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      client.publish(willTopic, "1", true);
      Serial.print("Sending payload: ");
      Serial.println(getResetInfo);
    }
  }

}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.print("failed, rc=");
      Serial.print(client.state());

      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      client.loop();
    }
  } else {
    wifi_connect();
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
