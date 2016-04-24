#include <Arduino.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <FS.h>
/* --- */
#define SHA1_SIZE 20
/* -- */
extern "C" {
  typedef struct {
    uint32_t Intermediate_Hash[SHA1_SIZE / 4];  /* Message Digest */
    uint32_t Length_Low;                        /* Message length in bits */
    uint32_t Length_High;                       /* Message length in bits */
    uint16_t Message_Block_Index;               /* Index into message block array   */
    uint8_t Message_Block[64];                  /* 512-bit message blocks */
  } SHA1_CTX;

  void SHA1_Init(SHA1_CTX *);
  void SHA1_Update(SHA1_CTX *, const uint8_t * msg, int len);
  void SHA1_Final(uint8_t *digest, SHA1_CTX *);

#include "user_interface.h"
}
/* -- */
#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/gopro_setting.h"
#include "/usr/local/src/twitter_setting.h"
/* -- */
#define INFO_PRINT 0
#define DEBUG_PRINT 0
/* -- */
#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160
/* -- */
#define MAX_ATTEMPT_IN_EACH_PHASE 5
/* ---- */
// [PHASE]
// 0  - get gopro file list                 ---> gorpo
// 1  - get last file                       ---> gopro
// 2  - tweet / INIT / get  media_id [txt]  ---> WIFI
// 3  - tweet / APPEND               [img]  ---> WIFI
// 4  - tweet / FIN                  [txt]  ---> WIFI
// 5  - tweet / upload status / GET  [txt]  ---> WIFI
// 6  - tweet / update status        [txt]  ---> WIFI
// 7  - spiffs / remove file

/* -- */
// twitter
#define CONSUMER_KEY          ConsumerKey
#define CONSUMER_SECRET       ConsumerSecret
#define ACCESS_TOKEN          AccessToken
#define ACCESS_SECRET         AccessSecret

#define BASE_HOST              "api.twitter.com"
#define BASE_URL               "https://api.twitter.com/1.1/statuses/update.json"
#define BASE_URI               "/1.1/statuses/update.json"
#define HTTPSPORT              443

#define KEY_HTTP_METHOD        "POST"
#define KEY_CONSUMER_KEY       "oauth_consumer_key"
#define KEY_NONCE              "oauth_nonce"
#define KEY_SIGNATURE_METHOD   "oauth_signature_method"
#define KEY_TIMESTAMP          "oauth_timestamp"
#define KEY_TOKEN              "oauth_token"
#define KEY_VERSION            "oauth_version"
#define KEY_SIGNATURE          "oauth_signature"
#define KEY_MEDIA_ID           "media_id"
#define KEY_MEDIA_TYPE         "media_type"
#define KEY_MEDIA_IDS          "media_ids"

#define VALUE_SIGNATURE_METHOD "HMAC-SHA1"
#define VALUE_VERSION          "1.0"

#define KEY_STATUS             "status"
#define UPLOAD_COMMAND         "command"

#define UPLOAD_BASE_HOST       "upload.twitter.com"
#define UPLOAD_BASE_URL        "https://upload.twitter.com/1.1/media/upload.json"
#define UPLOAD_BASE_URI        "/1.1/media/upload.json"
#define UPLOAD_OAUTH_KEY       "oauth_body_hash"

#define UPLOAD_CMD_INIT        "INIT"
#define UPLOAD_CMD_APPEND      "APPEND"
#define UPLOAD_CMD_FINALIZE    "FINALIZE"

#define UPLOAD_MEDIA_TYPE      "image/jpeg"
#define UPLOAD_MEDIA_SIZE      "total_bytes"
#define UPLOAD_MEDIA_SEG_INDX  "segment_index"

/* ---- */
const char* api_fingerprint    = "D8 01 5B F4 6D FB 91 C6 E4 B1 B6 AB 9A 72 C1 68 93 3D C2 D9";
const char* upload_fingerprint = "95 00 10 59 C8 27 FD 2C D0 76 12 F7 88 35 64 21 F5 60 D3 E9";

/* -- */
const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASSWORD;
const char* goprossid     = GOPRO_SSID;
const char* gopropassword = GOPRO_PASSWORD;
const char* otapassword   = OTA_PASSWORD;
/* --- */
IPAddress time_server = MQTT_SERVER;

/* -- config ---*/
String gopro_dir  = "00000000";
String gopro_file = "00000000";
int gopro_size    = 0;
int twitter_phase = 0;
String media_id   = "000000000000000000";
int chunked_no    = 0;
int attempt_this  = 0;
/* -- */
int CHUNKED_FILE_SIZE = 146000; // 146KB
/* -- */
WiFiClientSecure client;
WiFiUDP udp;
/* ---- */
unsigned int localPort = 12390;
const int timeZone     = 9;
/* ---- */
uint32_t value_timestamp;
//uint32_t value_nonce;
String value_nonce;
/* ---- */
const char* value_status  = "esp-01 / gopro image dn / test.....";
/* ---- */
bool x;

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println("[WIFIE] connected. IP address: " + String(WiFi.localIP().toString()) + " hostname: " + WiFi.hostname() + "  SSID: " + WiFi.SSID());
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("[WIFIE] client lost connection");
      break;
    case WIFI_EVENT_STAMODE_CONNECTED:
      Serial.println("[WIFIE] client connected");
      break;
    case WIFI_EVENT_STAMODE_AUTHMODE_CHANGE:
      Serial.println("[WIFIE] client authentication mode changed.");
      break;
    //case WIFI_STAMODE_DHCP_TIMEOUT:  THIS IS A NEW CONSTANT ENABLE WITH UPDATED SDK
    //  Serial.println("[WIFIE] client DHCP timeout reached.");
    //break;
    case WIFI_EVENT_SOFTAPMODE_STACONNECTED:
      Serial.println("[WIFIE] accesspoint: new client connected. Clients: "  + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_STADISCONNECTED:
      Serial.println("[WIFIE] accesspoint: client disconnected. Clients: " + String(WiFi.softAPgetStationNum()));
      break;
    case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
      //Serial.println("[WIFIE] accesspoint: probe request received.");
      break;
  }
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  Serial.println("[CONFIG] --- save");
  Serial.printf("[CONFIG] gopro_dir:     %s\n", gopro_dir.c_str());
  Serial.printf("[CONFIG] gopro_file:    %s\n", gopro_file.c_str());
  Serial.printf("[CONFIG] gopro_size:    %d\n", gopro_size);
  Serial.printf("[CONFIG] twitter_phase: %d\n", twitter_phase);
  Serial.printf("[CONFIG] media_id:      %s\n", media_id.c_str());
  Serial.printf("[CONFIG] chunked_no:    %d\n", chunked_no);
  Serial.printf("[CONFIG] attempt_this:   %d\n", attempt_this);
  Serial.println("[CONFIG] -------------- ");

  json["goproDir"]     = gopro_dir;
  json["goproFile"]    = gopro_file;
  json["goproSize"]    = gopro_size;
  json["twitterPhase"] = twitter_phase;
  json["mediaId"]      = media_id;
  json["chunkedNo"]    = chunked_no;
  json["attemptThis"]  = attempt_this;

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

  gopro_dir     = json["goproDir"].asString();
  gopro_file    = json["goproFile"].asString();
  gopro_size    = json["goproSize"];
  twitter_phase = json["twitterPhase"];
  media_id      = json["mediaId"].asString();
  chunked_no    = json["chunkedNo"];
  attempt_this  = json["attemptThis"];

  Serial.println("[CONFIG] --- load");
  Serial.printf("[CONFIG] gopro_dir:     %s\n", gopro_dir.c_str());
  Serial.printf("[CONFIG] gopro_file:    %s\n", gopro_file.c_str());
  Serial.printf("[CONFIG] gopro_size:    %d\n", gopro_size);
  Serial.printf("[CONFIG] twitter_phase: %d\n", twitter_phase);
  Serial.printf("[CONFIG] media_id:      %s\n", media_id.c_str());
  Serial.printf("[CONFIG] chunked_no:    %d\n", chunked_no);
  Serial.printf("[CONFIG] attempt_this:   %d\n", attempt_this);
  Serial.println("[CONFIG] -------------- ");

  return true;
}

void saveConfig_helper() {
  if (!saveConfig()) {
    Serial.println("[CONFIG] Failed to save config");
  } else {
    Serial.println("[CONFIG] Config saved");
  }
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
  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
}

void spiffs_format() {

  Serial.println("[SPIFFS] format file system");
  SPIFFS.format();
  Serial.println("[SPIFFS] format done");
}

/* ----------------------------------- */
String make_signature(const char* secret_one, const char* secret_two, String base_string) {

  String signing_key = URLEncode(secret_one);
  signing_key += "&";
  signing_key += URLEncode(secret_two);

  //Serial.println(signing_key);

  uint8_t digestkey[32];
  SHA1_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) signing_key.c_str(), (int)signing_key.length());
  SHA1_Final(digestkey, &context);

  uint8_t digest[32];
  ssl_hmac_sha1((uint8_t*) base_string.c_str(), (int)base_string.length(), digestkey, SHA1_SIZE, digest);

  String oauth_signature = URLEncode(base64::encode(digest, SHA1_SIZE).c_str());

  Serial.print("[Oauth] oauth_signature : ");
  Serial.println(oauth_signature);

  return oauth_signature;
}

// from http://hardwarefun.com/tutorials/url-encoding-in-arduino
// modified by chaeplin
String URLEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";

  while (*msg != '\0') {
    if ( ('a' <= *msg && *msg <= 'z')
         || ('A' <= *msg && *msg <= 'Z')
         || ('0' <= *msg && *msg <= '9')
         || *msg  == '-' || *msg == '_' || *msg == '.' || *msg == '~' ) {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 0xf];
    }
    msg++;
  }
  return encodedMsg;
}

// https://github.com/igrr/axtls-8266/blob/master/crypto/hmac.c
void ssl_hmac_sha1(const uint8_t *msg, int length, const uint8_t *key, int key_len, uint8_t *digest) {
  SHA1_CTX context;
  uint8_t k_ipad[64];
  uint8_t k_opad[64];
  int i;

  memset(k_ipad, 0, sizeof k_ipad);
  memset(k_opad, 0, sizeof k_opad);
  memcpy(k_ipad, key, key_len);
  memcpy(k_opad, key, key_len);

  for (i = 0; i < 64; i++)
  {
    k_ipad[i] ^= 0x36;
    k_opad[i] ^= 0x5c;
  }

  SHA1_Init(&context);
  SHA1_Update(&context, k_ipad, 64);
  SHA1_Update(&context, msg, length);
  SHA1_Final(digest, &context);
  SHA1_Init(&context);
  SHA1_Update(&context, k_opad, 64);
  SHA1_Update(&context, digest, SHA1_SIZE);
  SHA1_Final(digest, &context);
}

/* -------------------------- */
void print_FreeHeap() {
  Serial.print("[HEAP] mills : ");
  Serial.print(millis());
  Serial.print(" - heap : ");
  Serial.println(ESP.getFreeHeap());
  Serial.flush();
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime() {
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(time_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0;
}

void sendNTPpacket(IPAddress & address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void digitalClockDisplay() {
  Serial.print("[TIME] ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.print(" ");
}

void printDigits(int digits) {
  Serial.print(": ");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/* --------------------------------------- */
void setup() {
  system_update_cpu_freq(SYS_CPU_80MHz);
  WiFi.mode(WIFI_OFF);
  WiFi.onEvent(WiFiEvent);

  Serial.begin(115200);
  Serial.println("");
  Serial.flush();

  if (!SPIFFS.begin()) {
    Serial.println("[SPIFFS] Failed to mount file system");
    return;
  }

  //spiffs_format();

  if (!loadConfig()) {
    Serial.println("[CONFIG] Failed to load config");
  } else {
    Serial.println("[CONFIG] Config loaded");
  }

  // check fie size in config
  if ( twitter_phase != 0 && gopro_size == 0) {
    attempt_this  = 0;
    twitter_phase = 0;
    saveConfig_helper();
    delay(200);
    ESP.reset();
  }


  /** control phase **/

  twitter_phase = 3;
  chunked_no    = 0;

  /* -------------- */


  if ( twitter_phase == 0) {
    Serial.println("[FILE] removing files / start");
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      Serial.println(dir.fileName());
      if ( dir.fileName().startsWith("/GOPR")) {
        Serial.println("[FILE] remove this file");
        SPIFFS.remove(dir.fileName());
      }
      if ( dir.fileName().startsWith("/0416")) {
        Serial.println("[FILE] remove this file");
        SPIFFS.remove(dir.fileName());
      }
    }
    Serial.println("[FILE] removing files / done");
  }

  // switch wifi
  switch (twitter_phase) {

    case 0:
      gopro_connect();
      break;

    case 1:
      gopro_connect();
      break;

    default:
      wifi_connect();
      while (WiFi.status() != WL_CONNECTED) {
        delay(100);
      }
      udp.begin(localPort);

      setSyncProvider(getNtpTime);

      if (timeStatus() == timeNotSet) {
        setSyncProvider(getNtpTime);
      }
      break;
  }

  print_FreeHeap();

  x = true;
  Serial.flush();

  Serial.printf("[CONFIG] gopro_dir:     %s\n", gopro_dir.c_str());
  Serial.printf("[CONFIG] gopro_file:    %s\n", gopro_file.c_str());
  Serial.printf("[CONFIG] gopro_size:    %d\n", gopro_size);
  Serial.printf("[CONFIG] twitter_phase: %d\n", twitter_phase);
  Serial.printf("[CONFIG] media_id:      %s\n", media_id.c_str());
  Serial.printf("[CONFIG] chunked_no:    %d\n", chunked_no);
  Serial.printf("[CONFIG] attempt_this:   %d\n", attempt_this);
  Serial.println();
}

time_t prevDisplay = 0;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {

    if (now() != prevDisplay) {
      prevDisplay = now();
      if (timeStatus() == timeSet) {

        //digitalClockDisplay();
        //print_FreeHeap();

      } else {

        print_FreeHeap();

      }
    }

    if (x) {
      // switch todo

      switch (twitter_phase) {

        case 0:
          if (!get_gpro_list()) {
            attempt_this++;
            delay(200);
            saveConfig_helper();
          }
          break;

        case 1:
          if (!get_gopro_file()) {
            attempt_this++;
            saveConfig_helper();
            delay(200);
            ESP.reset();
          }
          break;

        default:
          break;

      }


      if (timeStatus() == timeSet) {
        /*
          value_timestamp  = now();
          value_nonce      = *(volatile uint32_t *)0x3FF20E44;
        */

        value_timestamp = 1461481166;
        value_nonce     = "NXsauV4OeiBHAGSptGOc6ZdqpJ1gLP63i0TyV6cWFs";
        media_id        = "724130611196092416";
        chunked_no      = 1;


        switch (twitter_phase) {

          case 2:
            tweet_init();
            break;

          case 3:
            tweet_append();
            break;


          /*
                    case 4:
                      tweet_fin();
                      break;

                    case 6:
                      tweet_status();
                      break;
          */

          default:
            x = false;
            break;
        }
      }
    }
    //x = false;
  } else {
    //wifi_connect();
    //setSyncProvider(getNtpTime);
  }
  delay(1000);
}

/* ------------------------------- */
String get_hash_str(String content_more, String content_last, int positionofchunk, int get_size, bool bpost = false) {

  digitalClockDisplay();
  print_FreeHeap();

  File f = SPIFFS.open("/" + gopro_file, "r");
  if (!f) {
    Serial.println("[SPIFFS] File doesn't exist yet");
    return "0";
  } else {
    f.seek(sizeof(float)*positionofchunk, SeekSet);
    Serial.printf("[SPIFFS] move position to %d\n", positionofchunk);
  }

  Serial.printf("[HASH] content_more size : %d\n", content_more.length());
  Serial.printf("[HASH] content_last size : %d\n", content_last.length());
  Serial.printf("[HASH] get_size : %d\n", get_size);

  if ( !bpost ) {

    char buff[1460] = { 0 };
    int len = get_size; // file size

    uint8_t digestkey[32];
    SHA1_CTX context;
    SHA1_Init(&context);

    SHA1_Update(&context, (uint8_t*) content_more.c_str(), content_more.length());

    while (f.available()) {
      int c = f.readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
 
      if ( c > 0 ) {
        SHA1_Update(&context, (uint8_t*) buff, c);
      }
 
      if (len > 0) {
        len -= c;
      }
  
      if (c == 0) {
        break;
      }
    }
    f.close();

    SHA1_Update(&context, (uint8_t*) content_last.c_str(), content_last.length());
    SHA1_Final(digestkey, &context);

    Serial.println("[HASH] Body hashing done");
    digitalClockDisplay();
    print_FreeHeap();

    return URLEncode(base64::encode(digestkey, 20).c_str());
  } else {

    char buff[1460] = { 0 };
    int len = get_size;

    digitalClockDisplay();

    client.print(content_more);

    Serial.println();

    while (f.available()) {
      int c = f.readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
      if ( c > 0 ) {
        client.write((const uint8_t *) buff, c);
      }

      digitalClockDisplay();

      Serial.printf("*****\t%d\t***\t%d %\n", (get_size - len), ((get_size - len) / (get_size / 100)));

      if (len > 0) {
        len -= c;
      }

      if (c == 0) {
        break;
      }

      if (!client.connected()) {
        return "0";
        break;
      }

    }
    client.print(content_last);
    f.close();
    return "1";
  }
}

bool do_http_append_post(String content_header, String content_more, String content_last, int positionofchunk, int get_size) {

  digitalClockDisplay();
  print_FreeHeap();
  Serial.print("[HTTP] start millis     : ");
  Serial.println(millis());

  Serial.print("connecting to ");
  Serial.println(UPLOAD_BASE_HOST);
  if (!client.connect(UPLOAD_BASE_HOST, HTTPSPORT)) {
    Serial.println("connection failed");
    return false;
  }
  client.setNoDelay(true);

  if (client.verify(upload_fingerprint, UPLOAD_BASE_HOST)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  Serial.print("requesting URL: ");
  Serial.println(UPLOAD_BASE_URI);

  // post image
  client.print(content_header);
  get_hash_str(content_more, content_last, positionofchunk, get_size, true);

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println(line);
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');

  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");

}

bool do_http_text_post(String OAuth_header) {
  String payload = "";

  String uri_to_post = UPLOAD_BASE_URI;
  if (twitter_phase == 6) {
    uri_to_post = BASE_URI;
    uri_to_post += "?media_ids=";
    uri_to_post += media_id;
  }

  Serial.printf("[HTTP] uri : %s\tphase : %d\n", uri_to_post.c_str(), twitter_phase);

  String req_body_to_post;

  HTTPClient http;
  switch (twitter_phase) {
    case 2:
      Serial.println("----> case 2 selected");
      req_body_to_post = "command=INIT&media_type=image%2Fjpeg&total_bytes=";
      req_body_to_post += gopro_size;
      http.begin(UPLOAD_BASE_HOST, HTTPSPORT, uri_to_post, upload_fingerprint);
      break;

    case 4:
      Serial.println("----> case 4 selected");
      req_body_to_post = "ommand=FINALIZE&media_id=";
      req_body_to_post +=  media_id;
      http.begin(UPLOAD_BASE_HOST, HTTPSPORT, uri_to_post, upload_fingerprint);
      break;

    case 6:
      Serial.println("----> case 6 selected");
      req_body_to_post = "status=";
      req_body_to_post += String(value_status);
      http.begin(BASE_HOST, HTTPSPORT, uri_to_post, api_fingerprint);
      break;

    default:
      Serial.println("----> no case selected");
      return false;
      break;
  }


  Serial.print("[HTTP] req_body_to_post size: ");
  Serial.println(req_body_to_post.length());

  Serial.print("[HTTP] req_body_to_post: ");
  Serial.println(req_body_to_post);

  http.addHeader("Authorization", OAuth_header);
  http.addHeader("Content-Length", String(req_body_to_post.length()));
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(req_body_to_post);
  if (httpCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
    //if (httpCode == HTTP_CODE_OK) {
    if (httpCode > 200 && httpCode < 400) {
      payload = http.getString();
      Serial.println("[HTTP] payload");
      Serial.println(payload);
    }
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
  http.end();

  if (twitter_phase == 6) {
    //if (httpCode == HTTP_CODE_OK) {
    if (httpCode > 200 && httpCode < 400) {
      attempt_this  = 0;
      twitter_phase = 7;
      saveConfig_helper();
      return true;
    } else {
      return false;
    }
  }

  if (twitter_phase == 2 || twitter_phase == 4) {
    char json[] = "{\"media_id\":000000000000000000,\"media_id_string\":\"000000000000000000\",\"size\":0000000,\"expires_after_secs\":86400,\"image\":{\"image_type\":\"image\\/jpeg\",\"w\":0000,\"h\":0000}}" ;
    payload.toCharArray(json, 200);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success()) {
      Serial.println("Failed to parse http payload");
      return false;
    }

    if (twitter_phase == 2) {
      if (root.containsKey("media_id_string")) {
        media_id = root["media_id_string"].asString();
        attempt_this  = 0;
        twitter_phase = 3;
        saveConfig_helper();
        return true;
      }
    } else {
      if (root.containsKey("media_id_string")) {
        if (media_id == root["media_id_string"]) {
          if (root.containsKey("processing_info")) {
            attempt_this  = 0;
            twitter_phase = 5;
            saveConfig_helper();
            return true;
          } else {
            attempt_this  = 0;
            twitter_phase = 6;
            saveConfig_helper();
            return true;
          }
        }
      }
    }
  }
}

String make_OAuth_header(String oauth_signature, String hashStr = "0") {
  String OAuth_header = "OAuth ";
  if (twitter_phase == 3) {
    OAuth_header += UPLOAD_OAUTH_KEY;
    OAuth_header += "=\"";
    OAuth_header += hashStr;
    OAuth_header += "\", ";
  }

  OAuth_header += KEY_CONSUMER_KEY;
  OAuth_header += "=\"";
  OAuth_header += CONSUMER_KEY;
  OAuth_header += "\", ";
  OAuth_header += KEY_NONCE;
  OAuth_header += "=\"";
  OAuth_header += value_nonce;
  OAuth_header += "\", ";
  OAuth_header += KEY_SIGNATURE;
  OAuth_header += "=\"";
  OAuth_header += oauth_signature;
  OAuth_header += "\", ";
  OAuth_header += KEY_SIGNATURE_METHOD;
  OAuth_header += "=\"";
  OAuth_header += VALUE_SIGNATURE_METHOD;
  OAuth_header += "\", ";
  OAuth_header += KEY_TIMESTAMP;
  OAuth_header += "=\"";
  OAuth_header += value_timestamp;
  OAuth_header += "\", ";
  OAuth_header += KEY_TOKEN;
  OAuth_header += "=\"";
  OAuth_header += ACCESS_TOKEN;
  OAuth_header += "\", ";
  OAuth_header += KEY_VERSION;
  OAuth_header += "=\"";
  OAuth_header += VALUE_VERSION;
  OAuth_header += "\"";

  Serial.print("[Oauth] OAuth_header : ");
  Serial.println(OAuth_header);

  return OAuth_header;

}

String make_base_string(String para_string) {
  String base_string = KEY_HTTP_METHOD;
  base_string += "&";

  switch (twitter_phase) {
    case 6:
      base_string += URLEncode(BASE_URL);
      break;

    default:
      base_string += URLEncode(UPLOAD_BASE_URL);
      break;
  }

  base_string += "&";
  base_string += URLEncode(para_string.c_str());

  Serial.print("[Oauth] base_string : ");
  Serial.println(base_string);

  return base_string;
}

String make_para_string(String hashStr = "0") {
  String para_string;

  switch (twitter_phase) {
    case 2:
      para_string += URLEncode(UPLOAD_COMMAND);
      para_string += "=" ;
      para_string += URLEncode(UPLOAD_CMD_INIT);
      para_string += "&";

      para_string += URLEncode(KEY_MEDIA_TYPE);
      para_string += "=" ;
      para_string += URLEncode(UPLOAD_MEDIA_TYPE);
      para_string += "&";
      break;

    case 3:
      para_string += URLEncode(UPLOAD_OAUTH_KEY);
      para_string += "=" ;
      para_string += hashStr.c_str();
      para_string += "&";
      break;

    case 4:
      para_string += URLEncode(UPLOAD_COMMAND);
      para_string += "=" ;
      para_string += URLEncode(UPLOAD_CMD_FINALIZE);
      para_string += "&";

      para_string += URLEncode(KEY_MEDIA_ID);
      para_string += "=" ;
      para_string += URLEncode(media_id.c_str());
      para_string += "&";
      break;

    case 6:
      para_string += URLEncode(KEY_MEDIA_IDS);
      para_string += "=" ;
      para_string += URLEncode(media_id.c_str());
      para_string += "&";
      break;

    default:
      break;
  }


  para_string += KEY_CONSUMER_KEY;
  para_string += "=" ;
  para_string += CONSUMER_KEY;
  para_string += "&";

  para_string += KEY_NONCE;
  para_string += "=";
  para_string += value_nonce;
  para_string += "&";

  para_string += KEY_SIGNATURE_METHOD;
  para_string += "=";
  para_string += VALUE_SIGNATURE_METHOD;
  para_string += "&";

  para_string += KEY_TIMESTAMP;
  para_string += "=";
  para_string += value_timestamp;
  para_string += "&";

  para_string += KEY_TOKEN;
  para_string += "=";
  para_string += ACCESS_TOKEN;
  para_string += "&";

  para_string += KEY_VERSION;
  para_string += "=";
  para_string += VALUE_VERSION;



  switch (twitter_phase) {
    case 2:
      para_string += "&";
      para_string += UPLOAD_MEDIA_SIZE;
      para_string += "=";
      para_string += gopro_size;
      break;


    case 3:
      break;

    case 6:
      para_string += "&";
      para_string += KEY_STATUS;
      para_string += "=";
      para_string += URLEncode(value_status);
      break;

    default:
      break;
  }

  Serial.print("[Oauth] para_string : ");
  Serial.println(para_string);

  return para_string;
}


void tweet_status() {
  Serial.println();
  Serial.printf("[PHASE : 6] ======================= %d\n", twitter_phase);

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  twitter_phase++;
}

void tweet_check() {
  // after FINALIZE, check status if result of FINALIZE contain processing_info
  twitter_phase++;
}

void tweet_fin() {
  Serial.println();
  Serial.printf("[PHASE : 4] ======================= %d\n", twitter_phase);

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  twitter_phase = 6;
}

void tweet_append() {
  Serial.println();
  Serial.printf("[PHASE : 3] ======================= %d\t%d\n", twitter_phase, chunked_no);

  int get_size = CHUNKED_FILE_SIZE;
  int positionofchunk = (CHUNKED_FILE_SIZE * chunked_no);

  if (positionofchunk >= gopro_size) {
    twitter_phase = 4;
    saveConfig_helper();
    return;
  }

  if (( positionofchunk + get_size) > gopro_size) {
    get_size = gopro_size - positionofchunk;
  }

  String content_more = "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"command\"\r\n\r\n";
  content_more += "APPEND\r\n";

  content_more += "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"segment_index\"\r\n\r\n";
  content_more += String(chunked_no) + "\r\n";

  content_more += "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"media_id\"\r\n\r\n";
  content_more += media_id + "\r\n";

  content_more += "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"media\"; filename=\"GOPR1839-ab\"\r\n";
  //content_more += "Content-Disposition: form-data; name=\"media\"; filename=\"" + String(media_id) + ".jpg\"\r\n" ;
  content_more += "Content-Type: application/octet-stream\r\n\r\n";

  String content_last = "\r\n--00Twurl817862339941931672lruwT99--\r\n";

  int content_length     = get_size + content_more.length() + content_last.length();
  String hashStr         = get_hash_str(content_more, content_last, positionofchunk, get_size);

  Serial.print("[HASH] hashStr : ");
  Serial.println(hashStr);

  String para_string     = make_para_string(hashStr);
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature, hashStr);

  String content_header = "POST " + String(UPLOAD_BASE_URI) + " HTTP/1.1\r\n";
  content_header += "Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0\r\n";
  content_header += "User-Agent: esp8266_image_bot_by_chaeplin_V_0.1\r\n";
  content_header += "Content-Type: multipart/form-data, boundary=\"00Twurl817862339941931672lruwT99\"\r\n";
  content_header += "Authorization: " + OAuth_header + "\r\n";
  content_header += "Connection: close\r\n";
  content_header += "Host: " + String(UPLOAD_BASE_HOST) + "\r\n";
  content_header += "Content-Length: " + String(content_length) + "\r\n\r\n";


  Serial.println("[HEADER] content_header : ");
  Serial.println(content_header);

  Serial.println("[HEADER] content_more : ");
  Serial.println(content_more);

  Serial.println("[HEADER] content_last : ");
  Serial.println(content_last);


  /*
    bool rtn = do_http_append_post(content_header, content_more, content_last, positionofchunk, get_size);
    if (rtn) {
    Serial.println("[PHASE : 3] ======================= OK");
    } else {
    Serial.println("[PHASE : 3] ======================= FAIL");
    }
  */

  x = false;
  chunked_no++;
}

void tweet_init() {
  Serial.println();
  Serial.printf("[PHASE : 2] ======================= %d\n", twitter_phase);

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  bool rtn               = do_http_text_post(OAuth_header);

  if (rtn) {
    Serial.println("[PHASE : 2] ======================= OK");
  } else {
    Serial.println("[PHASE : 2] ======================= FAIL");
  }
}

/* case 1 : start : get last file */
bool get_gopro_file() {
  bool rtn;

  Serial.println();
  Serial.printf("[PHASE : 1] ======================= %d\n", twitter_phase);

  HTTPClient http;
  client.setNoDelay(true);
  Serial.println("[HTTP] begin...");

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
      uint8_t buff[1460] = { 0 };

      if ( len != gopro_size ) {
        Serial.println("[DNLD] file size is diffrent, check file list og gopro again");
        http.end();
        twitter_phase = 0;
        saveConfig_helper();
        return false;
      }

      Serial.printf("[HTTP] GET... size: %d\n", len);
      WiFiClient * stream = http.getStreamPtr();

      File f = SPIFFS.open("/" + gopro_file, "w");

      if (!f) {
        Serial.println("[DNLD] Failed to open SPIFF for dnld file for writing");
        return false;
      }

      Serial.println("[DNLD] Progress: ");
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          Serial.printf("*****\t%d\t***\t%d %\n", (gopro_size - len), ((gopro_size - len) / (gopro_size / 100)));
          f.write(buff, c);

          if (len > 0) {
            len -= c;
          }
        }
      }
      f.close();
      Serial.println();

      int Attempt = 0;
      while (!SPIFFS.open(String("/") + gopro_file, "r")) {
        Serial.println("[SPIFFS] waiting SPIFFS");
        delay(1000);
        Attempt++;
        if (Attempt == 5) {
          return false;
        }
      }

      attempt_this  = 0;
      twitter_phase = 2;
      saveConfig_helper();

      Serial.println();
      Serial.println("[HTTP] connection closed");
    }
    rtn = true;
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    rtn = false;
  }
  http.end();
  return rtn;
}

/* case 0 : start : get gopro file list */
bool get_gpro_list() {
  bool rtn;

  Serial.println();
  Serial.printf("[PHASE : 0] ======================= %d\n", twitter_phase);

  HTTPClient http;
  Serial.println("[HTTP] begin...");
  http.begin("http://10.5.5.9:8080/gp/gpMediaList");
  Serial.println("[HTTP] GET...");

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

        gopro_dir     = directory.c_str();
        gopro_file    = filename.c_str();
        gopro_size    = filesize.toInt();
        media_id      = "000000000000000000";
        attempt_this  = 0;
        chunked_no    = 0;
        twitter_phase = 1;

        saveConfig_helper();

        Serial.printf("\n[HTTP] dir  : %s\n[HTTP] file : %s\n[HTTP] size : %s\n", directory.c_str(), filename.c_str(), filesize.c_str());

        Serial.println();
        Serial.println("[HTTP] connection closed or file end.");
      }
    }
    rtn = true;
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    rtn = false;
  }
  http.end();
  return rtn;
}

// end
