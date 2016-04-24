#include <Arduino.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <FS.h>

#define SHA1_SIZE 20
/* -- */
extern "C" {
  typedef struct {
    uint32_t Intermediate_Hash[SHA1_SIZE / 4]; /* Message Digest */
    uint32_t Length_Low;            /* Message length in bits */
    uint32_t Length_High;           /* Message length in bits */
    uint16_t Message_Block_Index;   /* Index into message block array   */
    uint8_t Message_Block[64];      /* 512-bit message blocks */
  } SHA1_CTX;

  void SHA1_Init(SHA1_CTX *);
  void SHA1_Update(SHA1_CTX *, const uint8_t * msg, int len);
  void SHA1_Final(uint8_t *digest, SHA1_CTX *);

#include "user_interface.h"
}

/* --- */
#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/gopro_setting.h"
#include "/usr/local/src/twitter_setting.h"

#define INFO_PRINT 0
#define DEBUG_PRINT 0

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

/* -- */
const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASSWORD;
const char* goprossid     = GOPRO_SSID;
const char* gopropassword = GOPRO_PASSWORD;
const char* otapassword   = OTA_PASSWORD;

/* --- */
String gopro_dir  = "00000000";
String gopro_file = "00000000";
int gopro_size   = 0;
int twitter_done = 0;

/* -- */
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

/* -- */
//WiFiClient client;
WiFiClientSecure client;


void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println("[WiFiEvent] connected. IP address: " + String(WiFi.localIP().toString()) + " hostname: " + WiFi.hostname() + "  SSID: " + WiFi.SSID());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("[WiFiEvent] client lost connection");
      break;
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.println("[WiFiEvent] client connected");
      break;
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
      Serial.println("[WiFiEvent] client authentication mode changed.");
      break;
    //case WIFI_STAMODE_DHCP_TIMEOUT:                             THIS IS A NEW CONSTANT ENABLE WITH UPDATED SDK
    //  Serial.println("[WiFiEvent] client DHCP timeout reached.");
    //break;
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      Serial.println("[WiFiEvent] accesspoint: new client connected. Clients: "  + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.println("[WiFiEvent] accesspoint: client disconnected. Clients: " + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      //Serial.println("[WiFiEvent] accesspoint: probe request received.");
      break;
  }
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  Serial.print("[CONFIG] gopro_dir: ");
  Serial.println(gopro_dir);
  Serial.print("[CONFIG] gopro_file: ");
  Serial.println(gopro_file);
  Serial.print("[CONFIG] gopro_size: ");
  Serial.println(gopro_size);
  Serial.print("[CONFIG] twitter_done: ");
  Serial.println(twitter_done);

  json["goproDir"]    = gopro_dir;
  json["goproFile"]   = gopro_file;
  json["goproSize"]   = gopro_size;
  json["twitterDone"] = twitter_done;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("[CONFIG] Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  return true;
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("[CONFIG] Failed to open config file");
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println("[CONFIG] Config file size is too large");
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    Serial.println("[CONFIG] Failed to parse config file");
    return false;
  }

  gopro_dir    = json["goproDir"].asString();
  gopro_file   = json["goproFile"].asString();
  gopro_size   = json["goproSize"];
  twitter_done = json["twitterDone"];

  Serial.print("[CONFIG] gopro_dir: ");
  Serial.println(gopro_dir);
  Serial.print("[CONFIG] gopro_file: ");
  Serial.println(gopro_file);
  Serial.print("[CONFIG] gopro_size: ");
  Serial.println(gopro_size);
  Serial.print("[CONFIG] twitter_done: ");
  Serial.println(twitter_done);

  return true;
}

void gopro_connect() {
  Serial.println("[WIFI] gopro start");
  //wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(goprossid, gopropassword);
}

void wifi_connect() {
  Serial.println("[WIFI] wifi start");
  //wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
}

void setup() {
  WiFi.mode(WIFI_OFF);
  WiFi.onEvent(WiFiEvent);

  Serial.begin(115200);
  Serial.println("");
  Serial.flush();

  if (!SPIFFS.begin()) {
    Serial.println("[SPIFFS] Failed to mount file system");
    return;
  }

  if (!loadConfig()) {
    Serial.println("[CONFIG] Failed to load config");

    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      Serial.print(dir.fileName());
      Serial.print(" ");
      File f = dir.openFile("r");
      Serial.println(f.size());
    }

    //Serial.println("[CONFIG] format file system");
    //SPIFFS.format();

  } else {
    Serial.println("[CONFIG] Config loaded");
  }

  if ( twitter_done != 0 && gopro_size == 0) {
    return;
  }

  if (twitter_done == 0 || twitter_done == 1) {
    gopro_connect();
  }

  Serial.flush();
}

void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    if (twitter_done == 0) {
      get_gpro_list();
    }

    if (twitter_done == 1) {
      get_gopro_file();
    }
  }

  delay(5000);
}

void get_gopro_file() {
  HTTPClient http;
  client.setNoDelay(1);
  Serial.print("[HTTP] begin...\n");

  String url = "http://10.5.5.9:8080/videos/DCIM/";
  url += gopro_dir;
  url += "/";
  url += gopro_file;

  Serial.print("[HTTP] GET... ");
  Serial.println(url);

  http.begin(url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      int len = http.getSize();
      uint8_t buff[1024] = { 0 };

      Serial.printf("[HTTP] GET... size: %d\n", len);
      WiFiClient * stream = http.getStreamPtr();

      File f = SPIFFS.open("/" + gopro_file, "w");

      if (!f) {
        Serial.println("[DNLD] Failed to open SPIFF for dnld file for writing");
        return;
      }
      
      Serial.println("Progress: ");
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

          Serial.print("********************* : ");
          Serial.println( gopro_size - len );
          f.write(buff, c);

          if (len > 0) {
            len -= c;
          }
        }
      }
      f.close();
      Serial.println();

      while(!SPIFFS.open(String("/") + gopro_file, "r")) {
        Serial.println("[SPIFFS] waiting SPIFFS");
        delay(1000);
      }

      twitter_done = 2;

      if (!saveConfig()) {
        Serial.println("[CONFIG] Failed to save config");
      } else {
        Serial.println("[CONFIG] Config saved");
      }

      Serial.println();
      Serial.println("[HTTP] connection closed");
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void get_gpro_list() {
  HTTPClient http;
  client.setNoDelay(1);
  Serial.print("[HTTP] begin...\n");
  http.begin("http://10.5.5.9:8080/gp/gpMediaList");
  Serial.print("[HTTP] GET...\n");

  int httpCode = http.GET();
  if (httpCode > 0) {

    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      while (http.connected()) {
        int len = http.getSize();
        WiFiClient * stream = http.getStreamPtr();

        String directory;
        String filename;
        String filesize;

        while (http.connected() && (len > 0 || len == -1)) {
          String line = stream->readStringUntil(',');

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

          //Serial.println(line);
        }

        gopro_dir    = directory.c_str();
        gopro_file   = filename.c_str();
        gopro_size   = filesize.toInt();
        twitter_done = 1;

        if (!saveConfig()) {
          Serial.println("[CONFIG] Failed to save config");
        } else {
          Serial.println("[CONFIG] Config saved");
        }

        Serial.println();
        Serial.print("[HTTP] dir  : ");
        Serial.println(directory);
        Serial.print("[HTTP] file : ");
        Serial.println(filename);
        Serial.print("[HTTP] size : ");
        Serial.println(filesize);

        Serial.println();
        Serial.print("[HTTP] connection closed or file end.\n");
      }
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}
// end
