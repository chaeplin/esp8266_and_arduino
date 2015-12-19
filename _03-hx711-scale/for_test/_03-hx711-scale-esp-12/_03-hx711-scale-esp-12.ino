#include <Wire.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <pgmspace.h>


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

//-------------------
volatile int measured = 0;
int inuse = LOW;
int r = LOW;
int o_r = LOW;

volatile int m = LOW;
int o_m = LOW;

char* topic  = "scale/test";

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

boolean reconnect()
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      if (DEBUG_PRINT) {
        Serial.println("===> mqtt connected");
      }
    } else {
      if (DEBUG_PRINT) {
        Serial.print("---> mqtt failed, rc=");
        Serial.println(client.state());
      }
    }
  }
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
    }
  } else {
    wifi_connect();
  }

  if ( m != o_m ) {
    hx711IsReady();
    o_m = m;

    if (DEBUG_PRINT) {
      Serial.print("measured = ");
      Serial.println(measured);
    }
  }

  if ( r != o_r )
  {
    if ( inuse == HIGH ) {
      digitalWrite(ledPin, HIGH);
    } else {
      digitalWrite(ledPin, LOW);
    }
    payload = "{\"scaletest\":";
    payload += measured;
    payload += ",\"millis\":";
    payload += (millis() - startMills);
    payload += "}";
    sendmqttMsg(topic, payload);
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
  int x;
  byte a, b, c;

  Wire.requestFrom(2, 3);

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();

  x = a;
  x = x << 8 | b;

  /*
    if ( x >= 7000 ) {
      x = 0;
    }
  */

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

void sendmqttMsg(char* topictosend, String payload)
{

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

    if ( client.publish(topictosend, p, msg_length)) {
      if (DEBUG_PRINT) {
        Serial.println("Publish ok");
      }
      free(p);
      //return 1;
    } else {
      if (DEBUG_PRINT) {
        Serial.println("Publish failed");
      }
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
//
