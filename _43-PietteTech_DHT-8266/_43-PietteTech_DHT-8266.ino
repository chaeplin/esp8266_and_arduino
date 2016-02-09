#include <ESP8266WiFi.h>
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
// for esp8266
// https://github.com/chaeplin/PietteTech_DHT-8266
#include "PietteTech_DHT.h"
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

// system defines
#define DHTTYPE  DHT22              // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   2              // Digital pin for communications
#define DHT_SAMPLE_INTERVAL   2000  // Sample every two seconds

#define REPORT_INTERVAL 2000 // in msec

String macToStr(const uint8_t* mac);
void sendmqttMsg(char* topictosend, String payload);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress mqtt_server = MQTT_SERVER;

char* topic = "pubtest";

String clientName;
long lastReconnectAttempt = 0;
unsigned long startMills;
bool acquired;
float t, h;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, wifiClient);

//declaration
void dht_wrapper(); // must be declared before the lib initialization

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
unsigned int DHTnextSampleTime;     // Next time we want to start sample
bool bDHTstarted;       // flag to indicate we started acquisition

// This wrapper is in charge of calling
// must be defined like this for the lib work
void dht_wrapper() {
  DHT.isrCallback();
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
  Serial.begin(115200);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);


  Serial.println(clientName);

  acquired = false;

  int result = DHT.acquireAndWait(1000);
  if ( result == DHTLIB_OK ) {
    t = DHT.getCelsius();
    h = DHT.getHumidity();
    acquired = true;
  } else {
    t = h = 0;
  }

  DHTnextSampleTime = 0;  // Start the first sample immediately

}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 200) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      if (millis() > DHTnextSampleTime) {
        if (!bDHTstarted) {
          DHT.acquire();
          bDHTstarted = true;
          acquired = false;
        }

        if (!DHT.acquiring()) {
          int result = DHT.getStatus();
          if ( result == 0 ) {
            t = DHT.getCelsius();
            h = DHT.getHumidity();

            bDHTstarted = false;
            acquired = true;
            DHTnextSampleTime = millis() + DHT_SAMPLE_INTERVAL;
          }
        }
      }

      if ((millis() - startMills) > REPORT_INTERVAL && acquired == true) {
        String payload ;
        payload += "{\"startMills\":";
        payload += millis();
        payload += ",\"Temperature\":";
        payload += t;
        payload += ",\"Humidity\":";
        payload += h;
        payload += ",\"FreeHeap\":";
        payload += ESP.getFreeHeap();
        payload += ",\"RSSI\":";
        payload += WiFi.RSSI();
        payload += "}";

        sendmqttMsg(topic, payload);
        startMills = millis();
      }
      client.loop();
    }
  } else {
    wifi_connect();
  }
}

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
