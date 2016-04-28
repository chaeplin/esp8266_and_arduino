// https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266HTTPClient/examples/StreamHttpClient/StreamHttpClient.ino

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#define USE_SERIAL Serial

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

bool bgetfiles;

void wifi_connect() {
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
}


void setup() {

  USE_SERIAL.begin(115200);
  // USE_SERIAL.setDebugOutput(true);
  while (!USE_SERIAL) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  USE_SERIAL.println();
  USE_SERIAL.println();
  USE_SERIAL.println();

  /*
    for (uint8_t t = 4; t > 0; t--) {
    USE_SERIAL.printf("[SETUP] WAIT %d...\n", t);
    USE_SERIAL.flush();
    delay(1000);
    }
  */
  bgetfiles = false;

  wifi_connect();
}

void loop() {
  // wait for WiFi connection
  if (WiFi.status() == WL_CONNECTED && !bgetfiles) {

    HTTPClient http;

    USE_SERIAL.print("[HTTP] begin...\n");

    // configure server and url
    http.begin("http://192.168.10.10/index.html");
    //http.begin("192.168.1.12", 80, "/test.html");

    USE_SERIAL.print("[HTTP] GET...\n");
    // start connection and send HTTP header
    int httpCode = http.GET();
    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled
      USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {

        // get lenght of document (is -1 when Server sends no Content-Length header)
        int len = http.getSize();

        // create buffer for read
        uint8_t buff[128] = { 0 };

        // get tcp stream
        WiFiClient * stream = http.getStreamPtr();

        // read all data from server
        while (http.connected() && (len > 0 || len == -1)) {
          // get available data size
          size_t size = stream->available();

          if (size) {
            // read up to 128 byte
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            // write it to Serial
            USE_SERIAL.write(buff, c);

            if (len > 0) {
              len -= c;
            }
          }
          delay(1);
        }
        bgetfiles = true;
        USE_SERIAL.println();
        USE_SERIAL.print("[HTTP] connection closed or file end.\n");

      }
    } else {
      USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  }

}
