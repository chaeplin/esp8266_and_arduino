#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "/usr/local/src/ap_setting.h"

#define DEBUG_PRINT 1
#define DEBUG_PRINT 1

#define MQTT_TEST_SERVER { 192, 168, 10, 144 }
#define MQTT_TEST_USER "test1"
#define MQTT_TEST_PASS "test123"

// *****************************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

const char* mqttuser = MQTT_TEST_USER;
const char* mqttpass = MQTT_TEST_PASS;

IPAddress mqtt_server = MQTT_TEST_SERVER;
const char* fingerprint = "70 B2 BF 0D 4E 2A 54 FC DD C3 75 03 CD 42 20 71 9C 4A 97 37";
//connecting to 192.168.10.144
//certificate matches

// test
//const char* fingerprint = "70 B2 BF 0D 4E 2A 54 FC DD C3 75 03 CD 42 20 71 9C 4A 97 36";
//connecting to 192.168.10.144
//certificate doesn't match
//-------------------------------
char* topic = "pubtest";

String clientName;

long lastReconnectAttempt = 0;
long lastMsg = 0;
int test_para = 2000;
unsigned long startMills;

//------------------------------
WiFiClientSecure wifiClient;
//WiFiClient wifiClient;
PubSubClient client(mqtt_server, 8883, wifiClient);

//----------------------
String macToStr(const uint8_t* mac);
void sendmqttMsg(char* topictosend, String payload);


void verifytls() {
  // Use WiFiClientSecure class to create TLS connection
  Serial.print("connecting to ");
  Serial.println(mqtt_server);
  if (!wifiClient.connect(mqtt_server, 8883)) {
    Serial.println("connection failed");
    return;
  }

  if (wifiClient.verify(fingerprint, mqtt_server.toString().c_str())) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }
}

  
//-----------------------
boolean reconnect()
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str(), mqttuser, mqttpass)) {
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

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED) {
    // WIFI
    if (DEBUG_PRINT) {
      Serial.println();
      Serial.print("===> WIFI ---> Connecting to ");
      Serial.println(ssid);
    }
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (DEBUG_PRINT) {
        Serial.print(". ");
        Serial.print(Attempt);
      }
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        if (DEBUG_PRINT) {
          Serial.println();
          Serial.println("-----> Could not connect to WIFI");
        }
        ESP.restart();
        delay(200);
      }

    }

    if (DEBUG_PRINT) {
      Serial.println();
      Serial.print("===> WiFi connected");
      Serial.print(" ------> IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}

void setup()
{
  startMills = millis();

  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  verifytls();
  //OTA
  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp-test");


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
    ESP.restart();
    /*
      if (error == OTA_AUTH_ERROR) abort();
      else if (error == OTA_BEGIN_ERROR) abort();
      else if (error == OTA_CONNECT_ERROR) abort();
      else if (error == OTA_RECEIVE_ERROR) abort();
      else if (error == OTA_END_ERROR) abort();
    */
  });

  ArduinoOTA.begin();
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 2000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      long now = millis();
      if (now - lastMsg > test_para) {
        lastMsg = now;
        String payload = "{\"startMills\":";
        payload += (millis() - startMills);
        payload += "}";
        sendmqttMsg(topic, payload);
      }
      client.loop();
      ArduinoOTA.handle();
    }
  } else {
    wifi_connect();
  }

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
