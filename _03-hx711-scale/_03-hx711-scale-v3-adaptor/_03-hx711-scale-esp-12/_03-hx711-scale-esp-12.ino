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
PubSubClient client(server, 1883, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

long lastReconnectAttempt = 0;

boolean reconnect() {
  if (client.connect((char*) clientName.c_str())) {
    Serial.println("connected");
    client.publish(hellotopic, "hello again 1 from ESP8266 s06");
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
  }
  return client.connected();
}

volatile int measured = 0;
volatile int inuse = LOW;
volatile int r = LOW;
int o_r = LOW;

int AvgMeasuredIsSent = LOW;
int nofchecked        = 0;
int nofnotinuse       = 0;
int measured_poop     = 0;
int measured_empty    = 0;

void setup() {
  Serial.begin(38400);
  Wire.begin(4, 5);
  Serial.println("HX711 START");
  delay(100);

  pinMode(nemoisOnPin, INPUT);
  pinMode(ledPin, OUTPUT);

  // WIFI
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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

  lastReconnectAttempt = 0;

  String getResetInfo = "hello from ESP8266 s06 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str())) {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        Serial.print("Sending payload: ");
        Serial.println(getResetInfo);
      }
    }
  }

  attachInterrupt(14, hx711IsReady, RISING);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
      Serial.print("failed, rc=");
      Serial.print(client.state());
    } else {
      client.loop();
    }
  } else {
    Serial.println("Could not connect to WIFI");
    ESP.restart();
  }

  if ( r != o_r )
  {
    ave.push(measured);

    if ( inuse == HIGH ) {
      digitalWrite(ledPin, HIGH);
      if ( measured > 20 )
      {
        if ( ((ave.maximum() - ave.minimum()) < 100 ) && ( ave.stddev() < 20) && ( nofchecked > 15 ) && ( ave.mean() > 1000 ) && ( ave.mean() < 7000 ) && ( AvgMeasuredIsSent == LOW ) )
        {
          payload = "{\"WeightAvg\":";
          payload += ( int(ave.mean()) - measured_empty );
          payload += ",\"WeightAvgStddev\":";
          payload += ave.stddev();
          payload += "}";

          sendHx711toMqtt(payload, topicAverage, 1);
        } else {
          if ( nofchecked > 3 )
          {
            payload = "{\"NemoWeight\":";
            payload += ( measured - measured_empty );
            payload += "}";

            sendHx711toMqtt(payload, topicEvery, 0);
          }
        }
      }
    } else {
      digitalWrite(ledPin, LOW);

      if ( ( ave.stddev() < 10 ) && ( nofnotinuse > 20 ) ) {
        if ( AvgMeasuredIsSent == HIGH )
        {
          Serial.print("poop_checked : ");
          Serial.println(int(ave.mean()));

          payload = "{\"WeightPoop\":";
          payload += ( int(ave.mean()) - measured_empty );
          payload += ",\"NemoWeight\":0}";
          sendHx711toMqtt(payload, topicEvery, 0);

          payload = "{\"WeightAvg\":0}";
          sendHx711toMqtt(payload, topicAverage, 0);

          AvgMeasuredIsSent = LOW;
        } else {
          payload = "{\"NemoEmpty\":";
          payload += int(ave.mean());
          payload += ",\"NemoEmptyStddev\":";
          payload += ave.stddev();
          payload += ",\"ScaleFreeHeap\":";
          payload += ESP.getFreeHeap();
          payload += ",\"ScaleRSSI\":";
          payload += WiFi.RSSI();
          payload += "}";

          measured_empty = int(ave.mean());
          sendHx711toMqtt(payload, topicEvery, 0);
        }
        nofnotinuse = 0;
      }

      nofchecked = 0;
    }
    nofchecked++;
    nofnotinuse++;
    o_r = r;
  }
}


void hx711IsReady()
{

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

  if ( x > 500 ) {
    inuse = HIGH;
  } else {
    inuse = LOW;
  }
  measured = x;
  r = !r;
}

void sendHx711toMqtt(String payload, char* topic, int retain)
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      client.publish(hellotopic, "hello again 2 from ESP8266 s06");
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.print(payload);

    unsigned int msg_length = payload.length();

    Serial.print(" length: ");
    Serial.println(msg_length);

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( retain == 1 ) {
      if ( client.publish(topic, p, msg_length, 1)) {
        if ( topic == "esp8266/arduino/s06" ) {
          AvgMeasuredIsSent = HIGH;
        }
        Serial.println("Publish ok");
        free(p);
      } else {
        Serial.println("Publish failed");
        free(p);
        abort();
      }
    } else {
      if ( client.publish(topic, p, msg_length)) {
        if ( topic == "esp8266/arduino/s06" ) {
          AvgMeasuredIsSent = HIGH;
        }
        Serial.println("Publish ok");
        free(p);
      } else {
        Serial.println("Publish failed");
        free(p);
        abort();
      }
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
