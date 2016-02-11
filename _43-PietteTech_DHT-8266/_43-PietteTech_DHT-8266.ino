// 80M CPU / 4M / 1M SPIFFS / esp-test-test
// with #define DHT_DEBUG_TIMING on PietteTech_DHT-8266
// #define DHTLIB_RESPONSE_MAX_TIMING 210
// #define DHTLIB_MAX_TIMING 165
#include <ESP8266WiFi.h>
// https://github.com/knolleary/pubsubclient
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
// for esp8266
// https://github.com/chaeplin/PietteTech_DHT-8266
#include "PietteTech_DHT.h"
#include "/usr/local/src/ap_setting.h"

// system defines
#define DHTTYPE  DHT22              // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   2              // Digital pin for communications

#define REPORT_INTERVAL 5000 // in msec

//#define DHT_DEBUG_TIMING 

String macToStr(const uint8_t* mac);
void sendmqttMsg(char* topictosend, String payload);
void sendUdpmsg(String msgtosend);
void printEdgeTiming(class PietteTech_DHT *_d);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;

char* topic = "pubtest";

unsigned int localPort = 2390;

String clientName;
long lastReconnectAttempt = 0;
unsigned long startMills;
float t, h;
int acquireresult;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, wifiClient);
WiFiUDP udp;

//declaration
void dht_wrapper(); // must be declared before the lib initialization

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
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

  //OTA
  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp-test-test");

  // No authentication by default
  ArduinoOTA.setPassword(otapassword);

  ArduinoOTA.onStart([]() {
    //Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //ESP.restart();
    if (error == OTA_AUTH_ERROR) abort();
    else if (error == OTA_BEGIN_ERROR) abort();
    else if (error == OTA_CONNECT_ERROR) abort();
    else if (error == OTA_RECEIVE_ERROR) abort();
    else if (error == OTA_END_ERROR) abort();

  });

  udp.begin(localPort);

  ArduinoOTA.begin();
  acquireresult = DHT.acquireAndWait(0);
  if ( acquireresult == 0 ) {
    t = DHT.getCelsius();
    h = DHT.getHumidity();
  } else {
    t = h = 0;
  }
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
      if (bDHTstarted) {
        if (!DHT.acquiring()) {
          acquireresult = DHT.getStatus();
#if defined(DHT_DEBUG_TIMING)           
          printEdgeTiming(&DHT);
#endif          
          if ( acquireresult == 0 ) {
            t = DHT.getCelsius();
            h = DHT.getHumidity();
          }
          bDHTstarted = false;
        }
      }

      if ((millis() - startMills) > REPORT_INTERVAL) {
        String payload ;
        payload += "{\"startMills\":";
        payload += millis();
        payload += ",\"Temperature\":";
        payload += t;
        payload += ",\"Humidity\":";
        payload += h;
        payload += ",\"acquireresult\":";
        payload += acquireresult;
        payload += ",\"FreeHeap\":";
        payload += ESP.getFreeHeap();
        payload += ",\"RSSI\":";
        payload += WiFi.RSSI();
        payload += "}";

        sendmqttMsg(topic, payload);

        startMills = millis();
        DHT.acquire();
        bDHTstarted = true;
      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}


void printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
#if defined(DHT_DEBUG_TIMING)    
  volatile uint8_t *_e = &_d->_edges[0];
#endif

  String udppayload = "edges2,device=esp-12-N2,debug=on ";
  for (n = 0; n < 41; n++) {
    char buf[2];
    if ( n < 40 ) {
      udppayload += "e";
      sprintf(buf, "%02d", n);
      udppayload += buf;
      udppayload += "=";
#if defined(DHT_DEBUG_TIMING)        
      udppayload += *_e++;
#endif         
      udppayload += "i,";
    } else {
      udppayload += "e";
      sprintf(buf, "%02d", n);
      udppayload += buf;
      udppayload += "=";
#if defined(DHT_DEBUG_TIMING)        
      udppayload += *_e++;
#endif         
      udppayload += "i";
    }
  }
  sendUdpmsg(udppayload);
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

void sendUdpmsg(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}
//
