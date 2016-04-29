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

WiFiClient client;

bool bgetfiles;

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println("[WIFI] connected. IP address: " + String(WiFi.localIP().toString()) + " hostname: " + WiFi.hostname() + "  SSID: " + WiFi.SSID());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("[WIFI] client lost connection");
      break;
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.println("[WIFI] client connected");
      break;
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
      Serial.println("[WIFI] client authentication mode changed.");
      break;
    //case WIFI_STAMODE_DHCP_TIMEOUT:                             THIS IS A NEW CONSTANT ENABLE WITH UPDATED SDK
    //  Serial.println("[WIFI] client DHCP timeout reached.");
    //break;
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      Serial.println("[WIFI] accesspoint: new client connected. Clients: "  + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.println("[WIFI] accesspoint: client disconnected. Clients: " + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      //Serial.println("[WIFI] accesspoint: probe request received.");
      break;
  }
}

void wifi_connect() {
  Serial.println("[WIFI] start");
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.flush();
  bgetfiles = true;
  wifi_connect();
}

void loop() {
  if (WiFi.status() == WL_CONNECTED && bgetfiles) {
    HTTPClient http;
    client.setNoDelay(1);
    Serial.print("[HTTP] begin...\n");
    http.begin("http://192.168.10.10/gopro1.json");
    Serial.print("[HTTP] GET...\n");

    int httpCode = http.GET();
    if (httpCode > 0) {

      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        while (http.connected()) {
          int len = http.getSize();
          WiFiClient * stream = http.getStreamPtr();

          Serial.println(len);

          String directory;
          String filename;
          String filesize;

          while (http.connected() && (len > 0 || len == -1)) {
            stream->setTimeout(10);
            String line = stream->readStringUntil(',');
            Serial.println(line);

            line.replace("\"", "");
            line.replace("media:[{", "");
            line.replace("fs:[", "");
            line.replace("{", "");
            line.replace("}", "");
            line.replace("]]", "");

            if (line.startsWith("d:")) {
              line.replace("d:", "");
              directory = line;
            }

            if (line.startsWith("n:")) {
              line.replace("n:", "");
              filename = line;
            }

            if (line.startsWith("s:")) {
              line.replace("s:", "");
              filesize = line;
            }

          }

          Serial.print("dir  : ");
          Serial.println(directory);
          Serial.print("file : ");
          Serial.println(filename);
          Serial.print("size : ");
          Serial.println(filesize);


          Serial.println();
          Serial.print("[HTTP] connection closed or file end.\n");
        }
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
    bgetfiles = false;
  }
}

