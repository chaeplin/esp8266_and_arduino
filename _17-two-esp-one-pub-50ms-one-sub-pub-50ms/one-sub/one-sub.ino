#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "/usr/local/src/ap_setting.h"

#define DEBUG_PRINT 1
#define EVENT_PRINT 1

char* topic = "pubtest";

String clientName;

long lastReconnectAttempt = 0;
long lastMsg = 0;
int test_para = 50;
unsigned long start2Mills;
unsigned long start3Mills = 0;


IPAddress server(192, 168, 10, 10);
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

/*
  if (EVENT_PRINT) {
    Serial.print(intopic);
    Serial.print(" => ");
    Serial.println(receivedpayload);
  }

  if (EVENT_PRINT) {
    Serial.print("-> receivedpayload 2 free Heap : ");
    Serial.println(ESP.getFreeHeap());
  }
*/
  parseMqttMsg(receivedpayload, receivedtopic);

}

void parseMqttMsg(String receivedpayload, String receivedtopic) {
  /*
  if (EVENT_PRINT) {
    Serial.print("-> jsonBuffer 1 free Heap : ");
    Serial.println(ESP.getFreeHeap());
  }

  if (EVENT_PRINT) {
    Serial.print("-> jsonBuffer 2 free Heap : ");
    Serial.println(ESP.getFreeHeap());
  }
  */

  char json[] = "{\"startMills\":12345678901234567890}";

  receivedpayload.toCharArray(json, 100);
  StaticJsonBuffer<100> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    if (EVENT_PRINT) {
      Serial.println("parseObject() failed");
    }
    return;
  }

  if (root.containsKey("startMills")) {
    start3Mills = root["startMills"];
  }
}


boolean reconnect()
{
  if (!client.connected()) {

    if (client.connect((char*) clientName.c_str())) {
      client.subscribe(topic);
      if (EVENT_PRINT) {
        Serial.println("===> mqtt connected");
      }
    } else {
      if (EVENT_PRINT) {
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
    if (EVENT_PRINT) {
      Serial.println();
      Serial.print("===> WIFI ---> Connecting to ");
      Serial.println(ssid);
    }
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (EVENT_PRINT) {
        Serial.print(". ");
        Serial.print(Attempt);
      }
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        if (EVENT_PRINT) {
          Serial.println();
          Serial.println("-----> Could not connect to WIFI");
        }
        ESP.restart();
        delay(200);
      }

    }

    if (EVENT_PRINT) {
      Serial.println();
      Serial.print("===> WiFi connected");
      Serial.print(" ------> IP address: ");
      Serial.println(WiFi.localIP());
    }
  }
}

void setup()
{
  start2Mills = millis();

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
      long now = millis();
      if (now - lastMsg > test_para) {
        lastMsg = now;
        String payload = "{\"start2Mills\":";
        payload += (millis() - start2Mills);
        payload += ",\"start3Mills\":";
        payload += start3Mills;
        payload += "}";
        sendmqttMsg(topic, payload);
      }
      client.loop();
    }
  } else {
    wifi_connect();
  }
  /*
    if ( millis() > 30000 ) {
      ESP.deepSleep(0);
      delay(250);
    }
    //delay(10);
  */

}

void sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    if (EVENT_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(payload);
    }

    unsigned int msg_length = payload.length();

    if (EVENT_PRINT) {
      Serial.print(" length: ");
      Serial.println(msg_length);
    }

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length)) {
      if (EVENT_PRINT) {
        Serial.println("Publish ok");
      }
      free(p);
      //return 1;
    } else {
      if (EVENT_PRINT) {
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
