#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Average.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define nemoisOnPin 14

Average<float> ave(10);

char* topicEvery   = "esp8266/arduino/s16";
char* topicAverage = "esp8266/arduino/s06";

char* hellotopic = "HELLO";

String clientName;
String payload;
WiFiClient wifiClient;

IPAddress server(192, 168, 10, 10);
PubSubClient client(wifiClient, server);

int AvgMeasuredIsSent = LOW;
int inuse             = LOW;
int measured          = 0;
int nofchecked        = 0;

void callback(const MQTT::Publish& pub) {
}

void setup() {
  Serial.begin(38400);
  Wire.begin(4, 5);
  Serial.println("HX711 START");
  delay(20);

  pinMode(nemoisOnPin, INPUT);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  client.set_callback(callback);

  WiFi.mode(WIFI_STA);

#ifdef __IS_MY_HOME
  WiFi.begin(ssid, password, channel, bssid);
  WiFi.config(IPAddress(192, 168, 10, 16), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
#else
  WiFi.begin(ssid, password);
#endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());


  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);

  if (client.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topicEvery);

    if (client.publish(hellotopic, "hello from ESP8266 s06")) {
      Serial.println("Publish ok");
    } else {
      Serial.println("Publish failed");
    }

  } else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }

}

void loop() {
  inuse = digitalRead(nemoisOnPin);
  if ( inuse == HIGH ) {
    measured = requestHX711();
    if (( measured > 500 ) && ( measured < 10000 ))
    {
      ave.push(measured);

      if ( ((ave.maximum() - ave.minimum()) < 100 ) && ( ave.stddev() < 50) && ( nofchecked > 10 ) && ( ave.mean() > 1000 ) && ( ave.mean() < 10000 ) && ( AvgMeasuredIsSent == LOW ) )
      {
        payload = "{\"WeightAvg\":";
        payload += int(ave.mean());
        payload += "}";

        sendHx711toMqtt(payload, topicAverage);
      } else {
        payload = "{\"NemoWeight\":";
        payload += measured;
        payload += "}";

        sendHx711toMqtt(payload, topicEvery);
      }
    }
  } else {
    measured       = 0;
    nofchecked     = 0;
    AvgMeasuredIsSent = LOW;
  }

  nofchecked++;
  client.loop();
  delay(500);
}

int requestHX711() {
  Wire.requestFrom(2, 3);

  int x;
  byte a, b;

  a = Wire.read();
  b = Wire.read();

  x = a;
  x = x << 8 | b;

  return x;
}


void sendHx711toMqtt(String payload, char* topic) {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again HX711");
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (client.publish(MQTT::Publish(topic, (char*) payload.c_str()).set_retain())) {
      Serial.println("Publish ok");
      if ( topic == "esp8266/arduino/s06" ) {
        AvgMeasuredIsSent = HIGH;
      }
    }
    else {
      Serial.println("Publish failed");
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

