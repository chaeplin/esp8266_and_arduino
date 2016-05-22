// esp-12 4M / 3M / esp-gopro / no ota
#include <Arduino.h>
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <base64.h>
#include <SPI.h>
#include <SD.h>
/* --- */
#define SHA1_SIZE 20
/* -- */
#define DEFAULT_DIR      "00000000"
#define DEFAULT_FILE     "00000000.000"
#define DEFAULT_MEDIA_ID "0000000000000000000";
// RTC
// https://github.com/Makuna/Rtc
#include <RtcDS3231.h>
// SD card
const int chipSelect = 16;
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
#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160
/* -- */
#define MAX_ATTEMPT_IN_EACH_PHASE 5
/* ---- */
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
// ****************
void callback(char* intopic, byte* inpayload, unsigned int length);
/* ---- */
const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASSWORD;
const char* goprossid     = GOPRO_SSID;
const char* gopropassword = GOPRO_PASSWORD;
const char* otapassword   = OTA_PASSWORD;
/* ---- */
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;
/* ---- */
struct {
  uint32_t hash;
  bool gopro_mode;
  float Temperature;
  int twitter_phase;
  int gopro_size;
  int chunked_no;
  int attempt_this;
  int pic_taken;
  char gopro_dir[32];
  char gopro_file[32];
  char media_id[32];
} rtc_boot_mode;
/* ---- */
//int CHUNKED_FILE_SIZE = 146000; // 146KB
//int CHUNKED_FILE_SIZE = 292000;
//int CHUNKED_FILE_SIZE = 140000;
int CHUNKED_FILE_SIZE = 112000;
//int CHUNKED_FILE_SIZE = 56000;

/* -- config ---*/
String gopro_dir  = DEFAULT_DIR;
String gopro_file = DEFAULT_FILE;
String media_id   = DEFAULT_MEDIA_ID;
/* -- */
String value_status;
/* ---- */
uint32_t value_timestamp;
uint32_t value_nonce;
/* ---- */
const char* subtopic = "radio/test/2"; //Outside temp
//
unsigned int localPort = 12390;
const int timeZone = 9;
//
String clientName;
String payload;
String getResetInfo;
bool ResetInfo = false;
////
WiFiClient wifiClient;
WiFiClientSecure sslclient;
//
PubSubClient mqttclient(mqtt_server, 1883, callback, wifiClient);
WiFiUDP udp;
RtcDS3231 Rtc;
//
long lastReconnectAttempt = 0;
volatile bool msgcallback;
//
bool x;
//
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length) {
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data) {
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

bool rtc_config_read() {
  bool ok = system_rtc_mem_read(65, &rtc_boot_mode, sizeof(rtc_boot_mode));
  uint32_t hash = calc_hash(rtc_boot_mode);
  if (!ok || rtc_boot_mode.hash != hash) {
    rtc_boot_mode.gopro_mode    = false;
    rtc_boot_mode.Temperature   = 0;
    rtc_boot_mode.gopro_size    = 0;
    rtc_boot_mode.twitter_phase = 0;
    rtc_boot_mode.chunked_no    = 0;
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.pic_taken     = 0;
    ok = false;
  } else {
    gopro_dir  = rtc_boot_mode.gopro_dir;
    gopro_file = rtc_boot_mode.gopro_file;
    media_id  = rtc_boot_mode.media_id;
  }
  return ok;
}

bool rtc_config_save() {
  strncpy(rtc_boot_mode.gopro_dir, gopro_dir.c_str(), sizeof(rtc_boot_mode.gopro_dir));
  strncpy(rtc_boot_mode.gopro_file, gopro_file.c_str(), sizeof(rtc_boot_mode.gopro_file));
  strncpy(rtc_boot_mode.media_id, media_id.c_str(), sizeof(rtc_boot_mode.media_id));
  rtc_boot_mode.hash = calc_hash(rtc_boot_mode);
  bool ok = system_rtc_mem_write(65, &rtc_boot_mode, sizeof(rtc_boot_mode));
  if (!ok) {
    ok = false;
  }
  return ok;
}

void saveConfig_helper() {
  if (!rtc_config_save()) {
    Serial.println("[CONFIG] save fail");
  } else {
    Serial.println("[CONFIG] saved");
  }
}

void gopro_connect() {
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(goprossid, gopropassword);

  Serial.print("[WIFI] conn to: ");
  Serial.println(goprossid);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("*");
    delay(100);
    Attempt++;
    if (Attempt == 300) {
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("[WIFI] ip: ");
  Serial.println(WiFi.localIP());
}

void wifi_connect() {
  //wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-gopro");

  Serial.print("[WIFI] conn to: ");
  Serial.println(ssid);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("*");
    delay(100);
    Attempt++;
    if (Attempt == 300) {
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("[WIFI] ip: ");
  Serial.println(WiFi.localIP());
}

/* ----------------------------------- */
String make_signature(const char* secret_one, const char* secret_two, String base_string) {

  String signing_key = URLEncode(secret_one);
  signing_key += "&";
  signing_key += URLEncode(secret_two);

  uint8_t digestkey[32];
  SHA1_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) signing_key.c_str(), (int)signing_key.length());
  SHA1_Final(digestkey, &context);

  uint8_t digest[32];
  ssl_hmac_sha1((uint8_t*) base_string.c_str(), (int)base_string.length(), digestkey, SHA1_SIZE, digest);

  String oauth_signature = URLEncode(base64::encode(digest, SHA1_SIZE).c_str());

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
/* -- */
void sendmqttMsg(char* topictosend, String payload) {
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (mqttclient.publish(topictosend, p, msg_length, 1)) {
    free(p);
  } else {
    free(p);
  }
  mqttclient.loop();
}

/* -- */
time_t requestSync() {
  return 0;
}

time_t requestRtc() {
  RtcDateTime Epoch32Time = Rtc.GetDateTime();
  return (Epoch32Time + 946684800);
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

void sendNTPpacket(IPAddress & address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

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

/* --- */
String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

boolean reconnect() {
  if (!mqttclient.connected()) {
    if (mqttclient.connect((char*) clientName.c_str())) {
      mqttclient.subscribe(subtopic);
      Serial.println("[MQTT] ---> mqttconnected");
    } else {
      Serial.print("[MQTT] ---> failed, rc=");
      Serial.println(mqttclient.state());
    }
  }
  return mqttclient.connected();
}

void callback(char* intopic, byte* inpayload, unsigned int length) {
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  Serial.print("[MQTT] ");
  Serial.print(intopic);
  Serial.print(" ====> ");
  Serial.print(receivedpayload);

  parseMqttMsg(receivedpayload, receivedtopic);
}

void parseMqttMsg(String receivedpayload, String receivedtopic) {
  char json[] = "{\"Humidity\":43.90,\"Temperature\":22.00,\"DS18B20\":22.00,\"PIRSTATUS\":0,\"FreeHeap\":43552,\"acquireresult\":0,\"acquirestatus\":0,\"DHTnextSampleTime\":2121587,\"bDHTstarted\":0,\"RSSI\":-48,\"millis\":2117963}";

  receivedpayload.toCharArray(json, 400);
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    return;
  }

  if ( receivedtopic == subtopic ) {
    if (root.containsKey("data1")) {
      rtc_boot_mode.Temperature = root["data1"];
    }
  }

  msgcallback = !msgcallback;
}

void setup() {
  Serial.begin(74880);
  system_update_cpu_freq(SYS_CPU_80MHz);
  WiFi.mode(WIFI_OFF);
  rtc_config_read();

  Wire.begin(0, 2);
  delay(100);

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  //
  lastReconnectAttempt = 0;
  msgcallback = false;

  getResetInfo = "hello from gopro ";
  getResetInfo += ESP.getResetInfo().substring(0, 80);

  Serial.println(getResetInfo);

  if (rtc_boot_mode.gopro_mode) {
    Serial.println("[Gopro mode]");
  } else {
    Serial.println("[Clock mode]");
  }

  if (rtc_boot_mode.gopro_mode) {
    if (!rtc_config_read()) {
      Serial.println("[CONFIG] load fail");
    } else {
      Serial.println("[CONFIG] loaded");
      Serial.print("[CONFIG] [PH] : ");
      Serial.println(rtc_boot_mode.twitter_phase);
    }

    // check fie size in config
    if ( rtc_boot_mode.twitter_phase != 0 && rtc_boot_mode.gopro_size == 0) {
      rtc_boot_mode.attempt_this  = 0;
      rtc_boot_mode.twitter_phase = 0;
      saveConfig_helper();
      Serial.flush();
      ESP.reset();
    }
  }

  if (!rtc_boot_mode.gopro_mode) {
    wifi_connect();
  } else {
    if ( rtc_boot_mode.twitter_phase > 1 ) {
      wifi_connect();
    } else {
      gopro_connect();
    }
  }

  if (!rtc_boot_mode.gopro_mode || rtc_boot_mode.twitter_phase > 1) {
    udp.begin(localPort);

    if (!Rtc.IsDateTimeValid()) {
      Serial.println("[RTC] --> Rtc.IsDateTime is inValid");

      setSyncProvider(requestSync);

      int Attempt = 0;
      while ( timeStatus() == timeNotSet ) {
        setSyncProvider(getNtpTime);
        Attempt++;
        if (Attempt > 3) {
          break;
        }
        yield();
      }

      if (timeStatus() == timeSet) {
        Serial.println("[RTC] --> ntp time synced");

        Rtc.SetDateTime(now() - 946684800);
        if (!Rtc.GetIsRunning()) {
          Rtc.SetIsRunning(true);
        }
      } else {
        Serial.println("[RTC] --> ntp not synced, use rtc time anyway");
      }
    } else {
      Serial.println("[RTC] --> Rtc.IsDateTime is Valid");
    }
  }

  setSyncProvider(requestRtc);
  setSyncInterval(60);

  Rtc.Enable32kHzPin(false);

  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmTwo);
  // Wakeup every hour
  DS3231AlarmTwo alarm1(
    0, // day
    0, // hour
    0, // min
    DS3231AlarmTwoControl_MinutesMatch);
  Rtc.SetAlarmTwo(alarm1);
  Rtc.LatchAlarmsTriggeredFlags();

  /*
    if (rtc_boot_mode.gopro_mode) {
    Serial.print("[SD] Initializing SD card...");
    // see if the card is present and can be initialized:
    if (!SD.begin(chipSelect)) {
      Serial.println("[SD] Card failed, or not present");
      // don't do anything more:
      //return;
      //go_to_sleep();
      ESP.reset();
    }
    Serial.println("[SD] card initialized.");
    }
  */
  x = true;
}

time_t prevDisplay = 0;

void loop() {
  if (!rtc_boot_mode.gopro_mode) {
    rtc_boot_mode.gopro_mode = true;
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 0;
    saveConfig_helper();
    delay(200);
    //go_to_sleep();
    ESP.reset();
  } else {

    if (WiFi.status() == WL_CONNECTED) {
      // gopro mode

      if (x) {
        // switch todo

        value_timestamp  = now();
        value_nonce      = *(volatile uint32_t *)0x3FF20E44;

        value_status  = "esp-12 / ";
        value_status += rtc_boot_mode.Temperature;
        value_status += "C / ";
        value_status += hour();
        value_status += ":";
        value_status += rtc_boot_mode.pic_taken;
        value_status += " / ";
        value_status += hour();
        value_status += ":";
        value_status += minute();

        switch (rtc_boot_mode.twitter_phase) {

          case 0:
            get_gpro_list();
            break;

          case 1:
            get_gopro_file();
            // off
            //gopro_power_on_off(false);
            Serial.flush();
            ESP.reset();
            break;
          /*
                    case 2:
                      tweet_init();
                      break;

                    case 3:
                      tweet_append();
                      break;

                    case 4:
                      tweet_fin();
                      break;

                    case 5:
                      tweet_check();
                      break;

                    case 6:
                      tweet_status();
                      break;

                    case 7:
                      rtc_boot_mode.gopro_mode = false;
                      saveConfig_helper();
                      Serial.flush();
                      ESP.reset();
                      break;
          */

          default:
            x = false;
            break;
        }
      }
    }
  }
}


/* ------------------------------- */
String get_hash_str(String content_more, String content_last, int positionofchunk, int get_size, bool bpost = false) {

  String fullPath = "DCIM/";
  fullPath += gopro_dir;
  fullPath += "/";
  fullPath += gopro_file;

  File f = SD.open(fullPath);

  if (!f) {
    return "0";
  } else {
    f.seek(positionofchunk);
  }

  if ( !bpost ) {

    char buff[1400] = { 0 };
    int len = get_size; // file size

    uint8_t digestkey[32];
    SHA1_CTX context;
    SHA1_Init(&context);

    SHA1_Update(&context, (uint8_t*) content_more.c_str(), content_more.length());

    while (f.available()) {
      int c = f.read(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));

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

    return URLEncode(base64::encode(digestkey, 20).c_str());
  } else {

    char buff[1400] = { 0 };
    int len = get_size;

    sslclient.print(content_more);

    Serial.println("[P:3] put : ");

    int pre_progress = 0;
    int count = 0;

    while (f.available()) {
      int c = f.read(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
      if ( c > 0 ) {
        sslclient.write((const uint8_t *) buff, c);
      }

      float progress = ((get_size - len) / (get_size / 100));
      if (int(progress) != pre_progress ) {
        if (progress < 10) {
          Serial.print(" ");
        }

        Serial.print(progress, 0);
        Serial.println(" % ");
        pre_progress = int(progress);
      }

      if (len > 0) {
        len -= c;
      }

      if (c == 0) {
        break;
      }

      if (!sslclient.connected()) {
        f.close();
        return "0";
        break;
      }

      Serial.print("[P:3] uld : ");
      Serial.println(count);
      count++;

      // up error
      if (count > 300) {
        rtc_boot_mode.attempt_this++;
        saveConfig_helper();
        Serial.flush();
        ESP.reset();
      }
    }

    sslclient.print(content_last);
    f.close();
    return "1";
  }
}

bool do_http_append_post(String content_header, String content_more, String content_last, int positionofchunk, int get_size) {
  if (!sslclient.connect(UPLOAD_BASE_HOST, HTTPSPORT)) {
    return false;
  }


  if (!sslclient.verify(upload_fingerprint, UPLOAD_BASE_HOST)) {
    Serial.println("[P:3] ssl fail");
    return false;
  }

  wifiClient.setNoDelay(true);
  sslclient.setNoDelay(true);

  sslclient.print(content_header);

  String ok = get_hash_str(content_more, content_last, positionofchunk, get_size, true);
  if (ok == "0") {
    Serial.println("[P:3] put err, reset");
    Serial.flush();
    ESP.reset();
  }

  int _returnCode = 0;
  while (sslclient.connected()) {
    String line = sslclient.readStringUntil('\n');
    if (line.startsWith("HTTP/1.")) {
      _returnCode = line.substring(9, line.indexOf(' ', 9)).toInt();
      break;
    }
  }

  if (_returnCode >= 200 && _returnCode < 400) {
    rtc_boot_mode.chunked_no++;
    saveConfig_helper();
    return true;
  } else {
    return false;
  }
}

bool do_http_text_post(String OAuth_header) {
  String payload = "";

  String uri_to_post = UPLOAD_BASE_URI;
  if (rtc_boot_mode.twitter_phase == 6) {
    uri_to_post = BASE_URI;
    uri_to_post += "?media_ids=";
    uri_to_post += media_id;
  }

  String req_body_to_post;

  HTTPClient http;

  Serial.print("case ");
  Serial.print(rtc_boot_mode.twitter_phase);
  Serial.print(" : ");
  switch (rtc_boot_mode.twitter_phase) {
    case 2:
      req_body_to_post = "command=INIT&media_type=image%2Fjpeg&total_bytes=";
      req_body_to_post += rtc_boot_mode.gopro_size;
      http.begin(UPLOAD_BASE_HOST, HTTPSPORT, uri_to_post, upload_fingerprint);
      break;

    case 4:
      req_body_to_post = "command=FINALIZE&media_id=";
      req_body_to_post +=  media_id;
      http.begin(UPLOAD_BASE_HOST, HTTPSPORT, uri_to_post, upload_fingerprint);
      break;

    case 6:
      req_body_to_post = "status=";
      req_body_to_post += String(URLEncode(value_status.c_str()));
      http.begin(BASE_HOST, HTTPSPORT, uri_to_post, api_fingerprint);
      break;

    default:
      return false;
      break;
  }

  http.addHeader("Authorization", OAuth_header);
  http.addHeader("Content-Length", String(req_body_to_post.length()));
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(req_body_to_post);
  Serial.print(httpCode);
  if (httpCode > 0) {
    if (httpCode >= 200 && httpCode < 400) {
      payload = http.getString();
    }
  } else {
    http.end();
    return false;
  }
  http.end();
  delay(1000);

  if (rtc_boot_mode.twitter_phase == 6) {
    if (httpCode >= 200 && httpCode < 400) {
      rtc_boot_mode.attempt_this  = 0;
      rtc_boot_mode.twitter_phase = 7;
      saveConfig_helper();
      delay(200);
      return true;
    } else {
      return false;
    }
  }

  if (rtc_boot_mode.twitter_phase == 2 || rtc_boot_mode.twitter_phase == 4) {
    char json[] = "{\"media_id\":000000000000000000,\"media_id_string\":\"000000000000000000\",\"size\":0000000,\"expires_after_secs\":86400,\"image\":{\"image_type\":\"image\\/jpeg\",\"w\":0000,\"h\":0000}}" ;
    payload.toCharArray(json, 200);
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success()) {
      return false;
    }

    if (rtc_boot_mode.twitter_phase == 2) {
      if (root.containsKey("media_id_string")) {
        media_id = root["media_id_string"].asString();
        rtc_boot_mode.attempt_this  = 0;
        rtc_boot_mode.twitter_phase = 3;
        saveConfig_helper();
        return true;
      }
    } else {
      if (root.containsKey("media_id_string")) {
        if (media_id == root["media_id_string"]) {
          if (root.containsKey("processing_info")) {
            rtc_boot_mode.attempt_this  = 0;
            rtc_boot_mode.twitter_phase = 5;
            saveConfig_helper();
            return true;
          } else {
            rtc_boot_mode.attempt_this  = 0;
            rtc_boot_mode.twitter_phase = 6;
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
  if (rtc_boot_mode.twitter_phase == 3) {
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

  return OAuth_header;

}

String make_base_string(String para_string) {
  String base_string = KEY_HTTP_METHOD;
  base_string += "&";

  switch (rtc_boot_mode.twitter_phase) {
    case 6:
      base_string += URLEncode(BASE_URL);
      break;

    default:
      base_string += URLEncode(UPLOAD_BASE_URL);
      break;
  }

  base_string += "&";
  base_string += URLEncode(para_string.c_str());

  return base_string;
}

String make_para_string(String hashStr = "0") {
  String para_string;

  switch (rtc_boot_mode.twitter_phase) {
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

  switch (rtc_boot_mode.twitter_phase) {
    case 2:
      para_string += "&";
      para_string += UPLOAD_MEDIA_SIZE;
      para_string += "=";
      para_string += rtc_boot_mode.gopro_size;
      break;


    case 3:
      break;

    case 6:
      para_string += "&";
      para_string += KEY_STATUS;
      para_string += "=";
      para_string += URLEncode(value_status.c_str());
      break;

    default:
      break;
  }

  return para_string;
}

bool tweet_status() {
  bool rtn = false;

  Serial.println("[P:6] TWEET");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  if (rtn) {
    Serial.println("[P:6] OK");
  } else {
    Serial.println("[P:6] FAIL");
  }

  return rtn;
}

void tweet_check() {
  rtc_boot_mode.twitter_phase++;
}

bool tweet_fin() {
  bool rtn = false;

  Serial.println("[P:4] FINALIZE");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  if (rtn) {
    Serial.println("[P:4] OK");
  } else {
    Serial.println("[P:4] FAIL");
  }
  return rtn;
}

bool tweet_append() {
  bool rtn = false;

  Serial.println("[P:3] APPEND");
  Serial.println("[P:3] chk : ");
  Serial.print(rtc_boot_mode.chunked_no);

  int get_size = CHUNKED_FILE_SIZE;
  int positionofchunk = (CHUNKED_FILE_SIZE * rtc_boot_mode.chunked_no);

  if (positionofchunk >= rtc_boot_mode.gopro_size) {
    rtc_boot_mode.twitter_phase = 4;
    saveConfig_helper();
    return true;
  }

  if (( positionofchunk + get_size) > rtc_boot_mode.gopro_size) {
    get_size = rtc_boot_mode.gopro_size - positionofchunk;
  }

  String content_more = "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"command\"\r\n\r\n";
  content_more += "APPEND\r\n";

  content_more += "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"segment_index\"\r\n\r\n";
  content_more += String(rtc_boot_mode.chunked_no) + "\r\n";

  content_more += "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"media_id\"\r\n\r\n";
  content_more += media_id + "\r\n";

  content_more += "--00Twurl817862339941931672lruwT99\r\n";
  content_more += "Content-Disposition: form-data; name=\"media\"; filename=\"" + String(value_timestamp) + ".jpg\"\r\n" ;
  content_more += "Content-Type: application/octet-stream\r\n\r\n";

  String content_last = "\r\n--00Twurl817862339941931672lruwT99--\r\n";

  int content_length = get_size + content_more.length() + content_last.length();
  String hashStr     = get_hash_str(content_more, content_last, positionofchunk, get_size);

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

  unsigned long upstart = millis();
  rtn = do_http_append_post(content_header, content_more, content_last, positionofchunk, get_size);
  float upspeed = get_size / (( millis() - upstart ) / 1000) ;

  Serial.println("[P:3] APPEND");
  if (rtn) {
    Serial.print("[P:3] chk : ");
    Serial.print(rtc_boot_mode.chunked_no - 1);
    Serial.println(" OK");
  } else {
    Serial.println(" FAIL");
  }

  Serial.print("[P:3] ");
  Serial.print(upspeed, 0);
  Serial.println(" KB");

  return rtn;
}

/* PHASE 2 */
bool tweet_init() {
  bool rtn = false;

  Serial.println("[P:2] INIT");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  if (rtn) {
    Serial.print("[P:2] ");
    Serial.println(media_id);
  } else {
    Serial.println("[P:2] FAIL");
  }
  return rtn;
}

/* PHASE 1 : start : get last file */
bool get_gopro_file() {
  bool rtn = false;
  int fileSize;

  String fullPath = "DCIM/";
  fullPath += gopro_dir;
  fullPath += "/";
  fullPath += gopro_file;

  Serial.print("[SD] Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("[SD] Card failed, or not present");
    // don't do anything more:
    //return;
    //go_to_sleep();
    ESP.reset();
  }
  Serial.println("[SD] card initialized.");

  File dataFile = SD.open(fullPath);
  if (dataFile) {
    fileSize = dataFile.size();
    dataFile.close();
  } else {
    Serial.println("[P1] can't open file");
  }

  if (fileSize == rtc_boot_mode.gopro_size) {
    Serial.println("[P1] filesize is ok");
    rtn = true;
  }

  if (rtn) {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 2;
    saveConfig_helper();
  } else {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 0;
    saveConfig_helper();
  }

  return rtn;
}


bool gopro_power_on_off(bool modedefault = true) {
  bool rtn = false;
  HTTPClient http;

  if (modedefault) {
    // on
    http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%01");
  } else {
    // off
    http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%00");
  }

  int httpCode = http.GET();

  Serial.print("[GOPRO] Power on code : ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  }

  http.end();
  return rtn;
}

bool gopro_wide_mode(bool modedefault = true) {
  bool rtn = false;
  HTTPClient http;

  // mode
  if (modedefault) {
    // 7MP WIDE
    http.begin("http://10.5.5.9:80/camera/PR?t=" + String(gopropassword) + "&p=%04");
  } else {
    // 5MP
    http.begin("http://10.5.5.9:80/camera/PR?t=" + String(gopropassword) + "&p=%03");
  }
  int httpCode = http.GET();

  Serial.print("[GOPRO] picture mode : ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  }

  http.end();
  return rtn;
}

bool gopro_shutter() {
  bool rtn = false;
  HTTPClient http;

  // shutter on
  http.begin("http://10.5.5.9:80/bacpac/SH?t=" + String(gopropassword) + "&p=%01");
  int httpCode = http.GET();

  Serial.print("[GOPRO] Shutter code : ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  }

  http.end();
  return rtn;
}

bool gopro_default() {
  bool rtn = false;
  HTTPClient http;

  // default mode : picture
  http.begin("http://10.5.5.9:80/camera/DM?t=" + String(gopropassword) + "&p=%01");
  int httpCode = http.GET();

  Serial.print("[GOPRO] Shutter code : ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  }

  http.end();
  return rtn;
}

bool gopro_autoshutdown() {
  bool rtn = false;
  HTTPClient http;

  // auto shutdown off
  http.begin("http://10.5.5.9:80/camera/AO?t=" + String(gopropassword) + "&p=%00");
  int httpCode = http.GET();

  Serial.print("[GOPRO] Shutter code : ");
  Serial.println(httpCode);

  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  }

  http.end();
  return rtn;
}

bool gopro_power() {
  bool rtn = false;
  if (gopro_power_on_off(true)) {
    delay(3000);
    /*
    if (gopro_wide_mode(true)) {
      delay(1000);
      if (gopro_default()) {
        delay(1000);
        if (gopro_autoshutdown()) {
          delay(1000);
          if (gopro_shutter()) {
            rtn = true;
          }
        }
      }
    }
    */
          if (gopro_shutter()) {
            rtn = true;
          }    
  }
  return rtn;
}

/* PHASE 0 : start : get gopro file list */
bool get_gpro_list() {
  bool rtn = false;

  Serial.println("[P:0] POWER ON ");
  int Attempt = 0;
  while (1) {
    if (gopro_power()) {
      Serial.println("---> OK");
      delay(5000);
      break;
    } else {
      delay(1000);
    }

    Attempt++;
    if (Attempt > 5) {
      Serial.println("---> FAIL");
      delay(1000);
      return false;
    }
  }

  Serial.print("[P:0] LIST ");

  HTTPClient http;
  http.begin("http://10.5.5.9:8080/gp/gpMediaList");
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      while (http.connected()) {
        int len = http.getSize();
        WiFiClient * stream = http.getStreamPtr();

        String directory;
        String filename;
        String filesize;

        while (http.connected() && (len > 0 || len == -1)) {
          //stream->setTimeout(500);
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
        }
        Serial.println(filesize.toInt());

        if ( filesize.toInt() < 2600000 ) {
          gopro_dir     = directory.c_str();
          gopro_file    = filename.c_str();
          media_id      = "0000000000000000000";
          rtc_boot_mode.gopro_size    = filesize.toInt();
          rtc_boot_mode.attempt_this  = 0;
          rtc_boot_mode.chunked_no    = 0;
          rtc_boot_mode.twitter_phase = 1;
          rtc_boot_mode.pic_taken     = minute();

          saveConfig_helper();

          rtn = true;
        } else {
          Serial.println("[P:0] big file");
          rtn = false;
        }
      }
    }
  } else {
    rtn = false;
  }
  http.end();

  if (rtn) {
    Serial.print("[P:0] ");
    Serial.println(gopro_file);
  } else {
    Serial.println("[P:0] FAIL");
  }
  return rtn;
}

// end
