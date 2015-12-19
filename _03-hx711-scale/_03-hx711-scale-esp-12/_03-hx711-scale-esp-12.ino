#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Average.h>
#include <pgmspace.h>
#include <Wire.h>

#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define nemoisOnPin 14
#define ledPin 13

#define DEBUG_PRINT 1

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
int32_t channel = WIFI_CHANNEL;

//
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

//-------------
int measured = 0;
int inuse = LOW;
int r = LOW;
int o_r = LOW;

volatile int m = LOW;
int o_m = LOW;

int AvgMeasuredIsSent = LOW;
int nofchecked        = 0;
int nofnotinuse       = 0;
int measured_poop     = 0;
int measured_empty    = 0;

Average<float> ave(10);

char* topicEvery   = "esp8266/arduino/s16";
char* topicAverage = "esp8266/arduino/s06";
char* hellotopic = "HELLO";

char* willTopic = "clients/scale";
char* willMessage = "0";

String clientName;
String payload;

// send reset info
String getResetInfo ;
int ResetInfo = LOW;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

unsigned long startMills;

long lastReconnectAttempt = 0;

void wifi_connect() {
  // WIFI
  if (DEBUG_PRINT) {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledPin, HIGH);
    delay(50);
    digitalWrite(ledPin, LOW);
    delay(50);
    Attempt++;
    if (DEBUG_PRINT) {
      Serial.print(".");
    }
    if (Attempt == 100)
    {
      if (DEBUG_PRINT) {
        Serial.println();
        Serial.println("Could not connect to WIFI");
      }
      ESP.restart();
      delay(2000);
    }
  }

  if (DEBUG_PRINT) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  //startMills = millis();

}

boolean reconnect() {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
      client.publish(willTopic, "1", true);
      if ( ResetInfo == LOW) {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        ResetInfo = HIGH;
      } else {
        client.publish(hellotopic, "hello again 1 from ESP8266 s06");
      }      
      if (DEBUG_PRINT) {
        Serial.println("connected");
      }
    } else {
      if (DEBUG_PRINT) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
      }
    }
  }
  //startMills = millis();
  return client.connected();
}

void setup() {
  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }
  delay(20);

  Wire.begin(4, 5);
  if (DEBUG_PRINT) {
    Serial.println("HX711 START");

    Serial.print("ESP.getChipId() : ");
    Serial.println(ESP.getChipId());

    Serial.print("ESP.getFlashChipId() : ");
    Serial.println(ESP.getFlashChipId());

    Serial.print("ESP.getFlashChipSize() : ");
    Serial.println(ESP.getFlashChipSize());
  }
  delay(100);

  startMills = millis();

  pinMode(nemoisOnPin, INPUT);
  pinMode(ledPin, OUTPUT);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;

  getResetInfo = "hello from ESP8266 s06 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

/*
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(willTopic, "1", true);
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        if (DEBUG_PRINT) {
          Serial.print("Sending payload: ");
          Serial.println(getResetInfo);
        }
      }
    } else {
      client.publish(willTopic, "1", true);
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      if (DEBUG_PRINT) {
        Serial.print("Sending payload: ");
        Serial.println(getResetInfo);
      }
    }
  }
*/
  attachInterrupt(14, check_isr, RISING);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (DEBUG_PRINT) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
      }

      unsigned long now = millis();
      if (now - lastReconnectAttempt > 1000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } /* else {
      client.loop();
    } */
  } else {
    wifi_connect();
  }

  if ( m != o_m ) {
    hx711IsReady();
    o_m = m;
  }

  if ( r != o_r )
  {
    ave.push(measured);

    if ( inuse == HIGH ) {
      digitalWrite(ledPin, HIGH);
      if ( measured > 200 )
      {
        if ( ( ave.stddev() < 15 ) && ( nofchecked > 15 ) && ( ave.mean() > 1000 ) && ( ave.mean() < 7000 ) && ( AvgMeasuredIsSent == LOW ) )
        {
          payload = "{\"WeightAvg\":";
          payload += ( int(ave.mean()) - measured_empty );
          payload += ",\"WeightStddev\":";
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
          if (DEBUG_PRINT) {
            Serial.print("poop_checked : ");
            Serial.println(int(ave.mean()));
          }

          payload = "{\"WeightPoop\":";
          payload += ( int(ave.mean()) - measured_empty );
          payload += ",\"NemoWeight\":0}";
          sendHx711toMqtt(payload, topicEvery, 0);
          /*
                    payload = "{\"WeightAvg\":0}";
                    sendHx711toMqtt(payload, topicAverage, 0);
          */
          AvgMeasuredIsSent = LOW;
        } else {
          payload = "{\"NemoEmpty\":";
          payload += int(ave.mean());
          payload += ",\"NemoStddev\":";
          payload += ave.stddev();
          payload += ",\"FreeHeap\":";
          payload += ESP.getFreeHeap();
          payload += ",\"RSSI\":";
          payload += WiFi.RSSI();
          payload += ",\"millis\":";
          payload += (millis() - startMills);
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
  client.loop();
}

void check_isr()
{
  m = !m;
}
void hx711IsReady()
{

  Wire.requestFrom(2, 3);

  int x;
  byte a, b, c;

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();

  // nedd to check value of a, b, c
  // result of Wire.read

  x = a;
  x = x << 8 | b;

  if ( x >= 7000 ) {
    x = 0;
  }

  if ( c == 1 ) {
    x = x * -1;
  }

  if ( x > 200 ) {
    inuse = HIGH;
  } else {
    inuse = LOW;
  }
  measured = x;
  r = !r;
}

void sendHx711toMqtt(String payload, char* topic, int retain)
{
  /*
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(willTopic, "1", true);
        client.publish(hellotopic, "hello again 2 from ESP8266 s06");
      }
    }
  */
  if (client.connected()) {
    if (DEBUG_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(payload);
    }

    unsigned int msg_length = payload.length();

    if (DEBUG_PRINT) {
      Serial.print(" length: ");
      Serial.println(msg_length);
    }
    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( retain == 1 ) {
      if ( client.publish(topic, p, msg_length, 1)) {
        if ( topic == "esp8266/arduino/s06" ) {
          AvgMeasuredIsSent = HIGH;
        }
        if (DEBUG_PRINT) {
          Serial.println("Publish ok");
        }
        free(p);
      } else {
        if (DEBUG_PRINT) {
          Serial.println("Publish failed");
        }
        free(p);
      }
    } else {
      if ( client.publish(topic, p, msg_length)) {
        if ( topic == "esp8266/arduino/s06" ) {
          AvgMeasuredIsSent = HIGH;
        }
        if (DEBUG_PRINT) {
          Serial.println("Publish ok");
        }
        free(p);
      } else {
        if (DEBUG_PRINT) {
          Serial.println("Publish failed");
        }
        free(p);
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
