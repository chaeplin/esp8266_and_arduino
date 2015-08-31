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
#define ledPin 13

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
int nofnotinuse       = 0;
int measured_blank    = 0;
int measured_poop     = 0;
int blank_checked     = LOW;
int measured_empty    = 0;

void setup() {
  Serial.begin(38400);
  Wire.begin(4, 5);
  Serial.println("HX711 START");
  delay(100);

  pinMode(nemoisOnPin, INPUT);
  pinMode(ledPin, OUTPUT);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  #ifdef __IS_MY_HOME
  WiFi.config(IPAddress(192, 168, 10, 16), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
  #endif

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledPin, HIGH);
    delay(50);
    digitalWrite(ledPin, LOW);
    delay(50);
    Attempt++;
    Serial.print(".");
    if (Attempt == 100)
    {
      Serial.println();
      Serial.println("Could not connect to WIFI");
      ESP.restart();
    }
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

}

void check_blank()
{
  for (int i = 0; i < 15; i++) {
    int blank_measured = requestHX711();
    if ( blank_measured > 500 ) {
      return ;
    }
    digitalWrite(ledPin, HIGH);
    ave.push(blank_measured);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
  }
  measured_blank = int(ave.mean());
  blank_checked = HIGH;
  Serial.print("blank_checked : ");
  Serial.println(measured_blank);

  payload = "{\"WeightBlank\":";
  payload += measured_blank;
  payload += "}";

  sendHx711toMqtt(payload, topicEvery);

}

void check_poop()
{
  for (int i = 0; i < 15; i++) {
    int poop_measured = requestHX711();
    digitalWrite(ledPin, HIGH);
    ave.push(poop_measured);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
  }

  measured_poop = int(ave.mean());
  Serial.print("poop_checked : ");
  Serial.println(measured_poop);

  payload = "{\"WeightPoop\":";
  payload += ( measured_poop - measured_empty );
  payload += ",\"NemoWeight\":";
  payload += 0;
  payload += "}";

  sendHx711toMqtt(payload, topicEvery);

  payload = "{\"WeightAvg\":0}";
  sendHx711toMqtt(payload, topicAverage);  

}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if  (
        client.connect(MQTT::Connect((char*) clientName.c_str()).set_clean_session().set_keepalive(120))) {
        client.publish(hellotopic, "hello from ESP8266 s06");
      }
    }

    if (client.connected()) {
      client.loop();
    } else {
      ESP.restart();
    }
  } else {
      Serial.println("Could not connect to WIFI");
      ESP.restart();    
  }
  
  if ( blank_checked == LOW ) {
    delay(2000);
    check_blank();
  }

  inuse = digitalRead(nemoisOnPin);
  measured = requestHX711();

  //Serial.print("measured : ");
  //Serial.println(measured);

  if ( inuse == HIGH ) {
    digitalWrite(ledPin, HIGH);
    if ( measured > 20 )
    {
      ave.push(measured);

      if ( ((ave.maximum() - ave.minimum()) < 50 ) && ( ave.stddev() < 20) && ( nofchecked > 15 ) && ( ave.mean() > 1000 ) && ( ave.mean() < 10000 ) && ( AvgMeasuredIsSent == LOW ) )
      {
        payload = "{\"WeightAvg\":";
        payload += ( int(ave.mean()) - measured_empty );
        payload += ",\"WeightAvgStddev\":";
        payload += ave.stddev();
        payload += "}";

        sendHx711toMqtt(payload, topicAverage);
      } else {
        if ( nofchecked > 3 ) {
          payload = "{\"NemoWeight\":";
          payload += ( measured - measured_empty );
          payload += "}";

          sendHx711toMqtt(payload, topicEvery);
        }
      }
    }
  } else {
    digitalWrite(ledPin, LOW);
    if ( AvgMeasuredIsSent == HIGH ) {
      check_poop();
      AvgMeasuredIsSent = LOW;
    }

    ave.push(measured);

    if ( ( ave.stddev() < 50) && ( nofnotinuse > 15 ) ) {
      payload = "{\"NemoEmpty\":";
      payload += int(ave.mean());
      payload += ",\"NemoEmptyStddev\":";
      payload += ave.stddev();
      payload += "}";

      sendHx711toMqtt(payload, topicEvery);

      if ( ave.stddev() < 10 ) {
        measured_empty = int(ave.mean());
      }

      measured    = 0;
      nofnotinuse = 0;
    }
    nofchecked = 0;
  }

  nofchecked++;
  nofnotinuse++;
  delay(1000);

}

int requestHX711() {
  Wire.requestFrom(2, 3);

  int x;
  byte a, b, c;

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();


  x = a;
  x = x << 8 | b;

  if ( x >= 7000 ) {
    x = 0;
  }

  if ( c == 1 ) {
    x = x * -1;
  }

  return x;
}


void sendHx711toMqtt(String payload, char* topic) 
{
  if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        if (
          client.connect(MQTT::Connect((char*) clientName.c_str())
                         .set_clean_session()
                         .set_will("status", "down")
                         .set_keepalive(60))
        ) {
          Serial.println("Connected to MQTT broker again HX711");
        }
        else {
          Serial.println("MQTT connect failed");
          Serial.println("Will reset and try again...");
          ESP.restart(); 
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
          ESP.restart(); 
        }
      }
  } else {
      Serial.println("Could not connect to WIFI");
      ESP.restart();    
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

