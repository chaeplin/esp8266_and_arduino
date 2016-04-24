// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/examples/StreamHttpClient/StreamHttpClient.ino

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include "FS.h"

extern "C" {
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

//
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

String clientName;
WiFiClient wifiClient;

const char* filename = "/0416.jpg";
bool bgetfiles;

void wifi_connect() {
  Serial.println("[WIFI] start");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300) {
      ESP.restart();
    }
  }
  Serial.println("[WIFI] connected");
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  bool result = SPIFFS.begin();
  Serial.println();
  Serial.println(" ");
  Serial.printf("[SPIFFS] opened: %d\n", result);

  File f = SPIFFS.open(filename, "r");
  if (f) {
    if (!SPIFFS.remove(filename)) {
      Serial.println("[SPIFFS] file remove failed");
    }
    Serial.println("[SPIFFS] download");
  } else {
    Serial.println("[SPIFFS] no file");
  }
  f.close();
  
  bgetfiles = true;

  wifi_connect();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && bgetfiles) {
    HTTPClient http;
    Serial.print("[HTTP] begin...\n");
    //http.begin("http://192.168.10.10/0416.jpg");
    http.begin("192.168.10.10", 80, "/0416.jpg");
    Serial.print("[HTTP] GET...\n");
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        int len = http.getSize();
        int filesize = len;
        uint8_t buff[128] = { 0 };

        Serial.printf("[HTTP] GET... size: %d\n", len);
        WiFiClient * stream = http.getStreamPtr();

        File f = SPIFFS.open(filename, "w");

        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            Serial.printf("*****\t%d\t***\t%d %\n", (filesize - len), ((filesize - len) / (filesize / 100)));
            //Serial.write(buff, c);
            f.write(buff, c);

            if (len > 0) {
              len -= c;
            }
          }
          delay(1);
        }
        f.close();

        Serial.println();
        Serial.println("[HTTP] connection closed");
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();

    Serial.println("[SPIFFS] verify");
    File f = SPIFFS.open(filename, "r");
    if (!f) {
      Serial.println("[SPIFFS] File doesn't exist yet");
    } else {
      
        Serial.println("[SPIFFS] content / start");
        while (f.available()) {
        String line = f.readStringUntil('\n');
        //Serial.println(line);
        }
      
      Serial.println("[SPIFFS] content / stop");
      Serial.println();
      Serial.printf("[SPIFFS] file.name(): %s\n", f.name());
      Serial.printf("[SPIFFS] file.size(): %d\n", f.size());
      f.close();
    }
  }
  bgetfiles = false;
}

