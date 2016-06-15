// esp12 4M / 3M flash / esp-solar / 160MHz
/*
  gopro wifi : https://github.com/KonradIT/goprowifihack
  twitter api : https://dev.twitter.com/rest/reference/post/media/upload
*/
#include <Arduino.h>
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <base64.h>
#include <FS.h>
/* --- */
#define SHA1_SIZE 20
/* -- */
#define DEFAULT_DIR      "00000000"
#define DEFAULT_FILE     "00000000.000"
#define DEFAULT_MEDIA_ID "0000000000000000000";
// RTC
// https://github.com/Makuna/Rtc
#include <RtcDS3231.h>
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

#define SquareWavePin 1
#define goproPowerPin 3
/* -- */
#define MAX_ATTEMPT_IN_EACH_PHASE 5
/* ---- */
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
  float Temperature1;
  float Temperature2;
  float Humidity;
  float data1;
  float data2;
  float data3;
  float data4;
  uint16_t powerAvg;
  uint16_t pir;
} solar_data;
/* ---- */
struct {
  uint32_t hash;
  bool gopro_mode;
  bool formatspiffs;
  float Temperature;
  int twitter_phase;
  int gopro_size;
  int chunked_no;
  int attempt_this;
  int attempt_phase;
  int attempt_detail;
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
char* hellotopic  = "HELLO";
char* willTopic   = "clients/solar";
char* willMessage = "0";

// subscribe
const char subrpi[]     = "raspberrypi/data";
const char subtopic_0[] = "esp8266/arduino/s03"; // lcd temp
const char subtopic_1[] = "esp8266/arduino/s07"; // power
const char subtopic_3[] = "radio/test/2";        //Outside temp
const char subtopic_4[] = "raspberrypi/doorpir";
const char subtopic_5[] = "raspberrypi/data2";

const char* substopic[6] = { subrpi, subtopic_0, subtopic_1, subtopic_3, subtopic_4, subtopic_5 } ;

unsigned int localPort = 12390;
const int timeZone = 9;

//
String clientName;
String payload;
String syslogPayload;
String getResetInfo;
bool ResetInfo = false;

/////////////
WiFiClient wifiClient;
WiFiClientSecure sslclient;

LiquidCrystal_I2C lcd(0x27, 20, 4);
PubSubClient mqttclient(mqtt_server, 1883, callback, wifiClient);
WiFiUDP udp;
RtcDS3231 Rtc;

long lastReconnectAttempt = 0;
volatile bool msgcallback;

//
volatile uint32_t lastTime, lastTime2;
volatile bool balm_isr;

// https://omerk.github.io/lcdchargen/
byte termometru[8]      = { B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110 };
byte picatura[8]        = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110 };
byte pirfill[8]         = { B00111, B00111, B00111, B00111, B00111, B00111, B00111, B00111 };
byte powericon[8]       = { B11111, B11011, B10001, B11011, B11111, B11000, B11000, B11000 };
byte customCharfill[8]  = { B10101, B01010, B10001, B00100, B00100, B10001, B01010, B10101 };
byte valueisinvalid[8]  = { B10000, B00000, B00000, B00000, B00000, B00000, B00000, B00000 };
byte customblock[8]     = { B00100, B00100, B00100, B00100, B00100, B00100, B00100, B00100 };

//
bool x;

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

void alm_isr() {
  balm_isr = true;
}

bool rtc_config_read() {
  bool ok = system_rtc_mem_read(65, &rtc_boot_mode, sizeof(rtc_boot_mode));
  uint32_t hash = calc_hash(rtc_boot_mode);
  if (!ok || rtc_boot_mode.hash != hash) {
    rtc_boot_mode.gopro_mode     = false;
    rtc_boot_mode.formatspiffs   = false;
    rtc_boot_mode.Temperature    = 0;
    rtc_boot_mode.gopro_size     = 0;
    rtc_boot_mode.twitter_phase  = 0;
    rtc_boot_mode.chunked_no     = 0;
    rtc_boot_mode.attempt_this   = 0;
    rtc_boot_mode.attempt_phase  = 0;
    rtc_boot_mode.attempt_detail = 0;
    rtc_boot_mode.pic_taken      = 0;
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

/*
  bool readConfig_helper() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (!rtc_config_read()) {
    lcd.print("[CONFIG] read fail");
  } else {
    lcd.print("[CONFIG] loaded");
  }
  delay(2000);
  }
*/

void saveConfig_helper() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (!rtc_config_save()) {
    lcd.print("[CONFIG] save fail");
  } else {
    lcd.print("[CONFIG] saved");
  }
  delay(2000);
}


void lcd_redraw() {
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.write(1);

  lcd.setCursor(0, 2);
  lcd.write(2);

  lcd.setCursor(0, 3);  // power
  lcd.write(6);

  lcd.setCursor(6, 2);  // b
  lcd.write(3);

  lcd.setCursor(6, 3);  // b
  lcd.write(3);
  //
  lcd.setCursor(6, 1);
  lcd.print((char)223);

  lcd.setCursor(12, 1);
  lcd.print((char)223);
}

void gopro_connect() {
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(goprossid, gopropassword);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("conn to: ");
  lcd.print(goprossid);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);

    int n = (Attempt % 20);
    lcd.setCursor(n, 1);
    lcd.print("*");
    if (n < 19) {
      lcd.setCursor((n + 1), 1);
      lcd.print(" ");
    }
    Attempt++;
    if (Attempt == 300) {
      rtc_boot_mode.attempt_this++;
      if ( rtc_boot_mode.attempt_this > 1) {
        rtc_boot_mode.attempt_phase = 0;
        rtc_boot_mode.attempt_detail = 1;
        rtc_boot_mode.twitter_phase = 8;
      }
      saveConfig_helper();
      delay(200);
      ESP.restart();
    }
  }
  lcd.setCursor(0, 2);
  lcd.print("ip: ");
  lcd.print(WiFi.localIP());
  delay(1000);
}

void wifi_connect() {

#define IPSET_STATIC { 192, 168, 10, 60 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

  IPAddress ip_static = IPSET_STATIC;
  IPAddress ip_gateway = IPSET_GATEWAY;
  IPAddress ip_subnet = IPSET_SUBNET;
  IPAddress ip_dns = IPSET_DNS;

  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));
  WiFi.hostname("esp-solar");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("conn to: ");
  lcd.print(ssid);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);

    int n = (Attempt % 20);
    lcd.setCursor(n, 1);
    lcd.print("*");
    if (n < 19) {
      lcd.setCursor((n + 1), 1);
      lcd.print(" ");
    }
    Attempt++;
    if (Attempt == 300) {
      ESP.restart();
    }
  }
  lcd.setCursor(0, 2);
  lcd.print("ip: ");
  lcd.print(WiFi.localIP());
  delay(1000);

  lcd_redraw();
}

void spiffs_format() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[SPIFFS] format ....");
  SPIFFS.format();
  lcd.setCursor(0, 1);
  lcd.print("[SPIFFS] format done");
  delay(1000);
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
void sendmqttMsg(char* topictosend, String topicpayload) {
  unsigned int msg_length = topicpayload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) topicpayload.c_str(), msg_length);

  if (mqttclient.publish(topictosend, p, msg_length, 1)) {
    free(p);
  } else {
    free(p);
  }
  mqttclient.loop();
}

void sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-solar: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void sendUdpmsg(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
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

      mqttclient.loop();

      for (int i = 0; i < 6; ++i) {
        mqttclient.subscribe(substopic[i]);
        mqttclient.loop();
      }
    } //else {
    //}
  }
  return mqttclient.connected();
}

void callback(char* intopic, byte* inpayload, unsigned int length) {
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }
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

  if ( receivedtopic == substopic[0] ) {
    if (root.containsKey("data1")) {
      solar_data.data1 = root["data1"];
    }
    if (root.containsKey("data2")) {
      solar_data.data2 = root["data2"];
    }
    lastTime = millis();
  }

  if ( receivedtopic == substopic[1] ) {
    if (root.containsKey("Humidity")) {
      solar_data.Humidity = root["Humidity"];
    }

    if (root.containsKey("Temperature")) {
      solar_data.Temperature1 = root["Temperature"];
    }
  }

  if ( receivedtopic == substopic[2] ) {
    if (root.containsKey("powerAvg")) {
      solar_data.powerAvg = root["powerAvg"];
    }
  }

  if ( receivedtopic == substopic[3] ) {
    if (root.containsKey("data1")) {
      solar_data.Temperature2 = root["data1"];
    }
  }

  if ( receivedtopic == substopic[4] ) {
    if (root.containsKey("DOORPIR")) {
      if (solar_data.pir != root["DOORPIR"]) {
        solar_data.pir  = int(root["DOORPIR"]);
      }
    }
  }

  if ( receivedtopic == substopic[5] ) {
    if (root.containsKey("data1")) {
      solar_data.data3 = root["data1"];
    }
    if (root.containsKey("data2")) {
      solar_data.data4 = root["data2"];
    }
    lastTime2 = millis();
  }

  msgcallback = !msgcallback;
}

void setup() {
  Serial.swap();
  system_update_cpu_freq(SYS_CPU_160MHz);
  WiFi.mode(WIFI_OFF);
  rtc_config_read();

  Wire.begin(0, 2);
  //twi_setClock(200000);
  //delay(100);

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  //
  lastReconnectAttempt = 0;
  msgcallback = false;

  getResetInfo = "hello from solar ";
  getResetInfo += ESP.getResetInfo().substring(0, 80);

  //
  solar_data.Temperature1 = solar_data.Temperature2 = solar_data.Humidity = 0;
  solar_data.data1 = solar_data.data2 = solar_data.data3 = solar_data.data4 = solar_data.powerAvg = 0;
  lastTime = lastTime2 = millis();

  // lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.createChar(1, termometru);
  lcd.createChar(2, picatura);
  lcd.createChar(3, customblock);
  lcd.createChar(4, customCharfill);
  lcd.createChar(5, pirfill);
  lcd.createChar(6, powericon);
  lcd.createChar(7, valueisinvalid);

  for ( int i = 1 ; i < 19 ; i++) {
    lcd.setCursor(i, 0);
    lcd.write(4);
    lcd.setCursor(i, 3);
    lcd.write(4);
  }

  for ( int i = 0 ; i < 4 ; i++) {
    lcd.setCursor(0, i);
    lcd.write(4);
    lcd.setCursor(19, i);
    lcd.write(4);
  }

  lcd.setCursor(4, 1);
  if (rtc_boot_mode.gopro_mode) {
    lcd.print("[Gopro mode]");
  } else {
    lcd.print("[Clock mode]");
  }

  delay(1000);
  lcd.clear();

  if (!SPIFFS.begin()) {
    lcd.setCursor(0, 0);
    lcd.print("[SPIFFS] mnt fail");
    delay(1000);
    return;
  }

  if (rtc_boot_mode.formatspiffs) {
    spiffs_format();
    rtc_boot_mode.formatspiffs = false;
    saveConfig_helper();
  }

  if (rtc_boot_mode.gopro_mode) {
    lcd.setCursor(0, 1);
    if (!rtc_config_read()) {
      lcd.print("[CONFIG] load fail");
    } else {
      lcd.print("[CONFIG] loaded");
      lcd.setCursor(0, 2);
      lcd.print("[CONFIG] [PH] : ");
      lcd.print(rtc_boot_mode.twitter_phase);
    }
    delay(1000);

    // check fie size in config
    if ( rtc_boot_mode.twitter_phase != 0 && rtc_boot_mode.gopro_size == 0 && rtc_boot_mode.twitter_phase != 8) {
      rtc_boot_mode.attempt_this  = 0;
      rtc_boot_mode.twitter_phase = 0;
      saveConfig_helper();
      delay(200);
      ESP.reset();
    }

    if ( rtc_boot_mode.twitter_phase == 0) {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        if ( dir.fileName().startsWith("/GOPR")) {
          SPIFFS.remove(dir.fileName());
        }
        if ( dir.fileName().startsWith("/config")) {
          SPIFFS.remove(dir.fileName());
        }
      }
    }
  }

  if (!rtc_boot_mode.gopro_mode || rtc_boot_mode.attempt_this > 4) {
    wifi_connect();
  } else {
    if ( rtc_boot_mode.twitter_phase > 1 ) {
      wifi_connect();
    } else {
      
      pinMode(goproPowerPin, OUTPUT);
      
      digitalWrite(goproPowerPin, LOW);
      delay(50);
      digitalWrite(goproPowerPin, HIGH);
      delay(50);
      digitalWrite(goproPowerPin, LOW);

      gopro_connect();
    }
  }

  if (!rtc_boot_mode.gopro_mode || rtc_boot_mode.twitter_phase > 1) {
    udp.begin(localPort);

    if (!Rtc.IsDateTimeValid()) {
      lcd.setCursor(0, 3);
      lcd.print("RTC is inValid");
      delay(1000);

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
        lcd.setCursor(0, 3);
        lcd.print("ntp synced");

        Rtc.SetDateTime(now() - 946684800);
        if (!Rtc.GetIsRunning()) {
          Rtc.SetIsRunning(true);
        }
      } //else {
      // }
    } else {
      lcd.setCursor(0, 3);
      lcd.print("RTC is Valid");
      delay(1000);
    }
  }

  setSyncProvider(requestRtc);
  setSyncInterval(60);

  Rtc.Enable32kHzPin(false);

  if (!rtc_boot_mode.gopro_mode) {
    /*
      Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmBoth);

      // Alarm 1 set to trigger every day when
      // the hours, minutes, and seconds match

      RtcDateTime alarmTime = now() - 946684800 + 128; // into the future
      DS3231AlarmOne alarm1(
      alarmTime.Day(),
      alarmTime.Hour(),
      alarmTime.Minute(),
      alarmTime.Second(),
      DS3231AlarmOneControl_HoursMinutesSecondsMatch);
      Rtc.SetAlarmOne(alarm1);

      // Alarm 2 set to trigger at the top of the minute
      DS3231AlarmTwo alarm2(
      0,
      0,
      0,
      DS3231AlarmTwoControl_OncePerMinute);
      Rtc.SetAlarmTwo(alarm2);

      // throw away any old alarm state before we ran
      Rtc.LatchAlarmsTriggeredFlags();
    */

    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeAlarmTwo);
    // Wakeup every hour
    DS3231AlarmTwo alarm1(
      0, // day
      0, // hour
      0, // min
      DS3231AlarmTwoControl_MinutesMatch);
    Rtc.SetAlarmTwo(alarm1);
    Rtc.LatchAlarmsTriggeredFlags();
  } else {
    Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  }

  //OTA
  if (!rtc_boot_mode.gopro_mode) {
    ArduinoOTA.setPort(8266);
    ArduinoOTA.setHostname("esp-solar");
    ArduinoOTA.setPassword(otapassword);
    ArduinoOTA.onStart([]() {
      sendUdpSyslog("ArduinoOTA Start");
    });
    ArduinoOTA.onEnd([]() {
      sendUdpSyslog("ArduinoOTA End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      syslogPayload = "Progress: ";
      syslogPayload += (progress / (total / 100));
      sendUdpSyslog(syslogPayload);
    });
    ArduinoOTA.onError([](ota_error_t error) {
      if (error == OTA_AUTH_ERROR) abort();
      else if (error == OTA_BEGIN_ERROR) abort();
      else if (error == OTA_CONNECT_ERROR) abort();
      else if (error == OTA_RECEIVE_ERROR) abort();
      else if (error == OTA_END_ERROR) abort();

    });

    ArduinoOTA.begin();

    lcd_redraw();

    pinMode(SquareWavePin, INPUT_PULLUP);
    attachInterrupt(1, alm_isr, FALLING);
  }

  x = true;
}

time_t prevDisplay = 0;

void loop() {
  if (!rtc_boot_mode.gopro_mode) {
    if (balm_isr) {
      DS3231AlarmFlag flag = Rtc.LatchAlarmsTriggeredFlags();
      if (flag & DS3231AlarmFlag_Alarm2) {
        rtc_boot_mode.gopro_mode  = true;
        rtc_boot_mode.Temperature = solar_data.Temperature2 ;
        rtc_boot_mode.attempt_this  = 0;
        rtc_boot_mode.twitter_phase = 0;
        rtc_boot_mode.attempt_phase = 0;
        rtc_boot_mode.attempt_detail = 0;
        saveConfig_helper();
        delay(200);

        ESP.reset();
      }
      balm_isr = false;
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttclient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > 100) {
          lastReconnectAttempt = now;
          if (reconnect()) {
            lastReconnectAttempt = 0;
          }
        }
      } else {
        if (timeStatus() != timeNotSet) {
          if (now() != prevDisplay) {
            prevDisplay = now();
            digitalClockDisplay();
            displayTemperature();
            displaypowerAvg();
            displaypir();

            if (second() % 10 == 0) {
              displayData();
              displayData2();

              if (( millis() - lastTime ) > 120000 ) {
                lcd.setCursor(7, 2);
                lcd.write(7);
              } else {
                lcd.setCursor(7, 2);
                lcd.print(" ");
              }

              if (( millis() - lastTime2 ) > 120000 ) {
                lcd.setCursor(7, 3);
                lcd.write(7);
              } else {
                lcd.setCursor(7, 3);
                lcd.print(" ");
              }
            }

            if (!Rtc.IsDateTimeValid()) {
              lcd.setCursor(18, 0);
              lcd.write(7);
            } else {
              lcd.setCursor(18, 0);
              lcd.print(" ");
            }

            if (msgcallback) {
              lcd.setCursor(19, 0);
              lcd.write(5);
            } else {
              lcd.setCursor(19, 0);
              lcd.print(" ");
            }
          }
        }
        mqttclient.loop();
      }
      ArduinoOTA.handle();
    } else {
      wifi_connect();
    }
  } else {

    if (WiFi.status() == WL_CONNECTED) {
      // gopro mode

      if (x) {
        // switch todo

        value_timestamp  = now();
        value_nonce      = *(volatile uint32_t *)0x3FF20E44;

        if ( rtc_boot_mode.twitter_phase == 8 ) {
          if (rtc_boot_mode.attempt_phase == 0 && rtc_boot_mode.attempt_detail == 1) {
            value_status  = "esp-01 / ";
            value_status += hour();
            value_status += ":";
            value_status += minute();
            value_status += " gopro wifi err, check plz";
          } else {
            value_status  = "esp-01 / ";
            value_status += hour();
            value_status += ":";
            value_status += minute();
            value_status += " PH: ";
            value_status += rtc_boot_mode.attempt_phase;
            value_status += " err, check plz";
          }
        } else {
          value_status  = "esp-01 / ";
          value_status += rtc_boot_mode.Temperature;
          value_status += "C / ";
          value_status += hour();
          value_status += ":";
          value_status += rtc_boot_mode.pic_taken;
          value_status += " / ";
          value_status += hour();
          value_status += ":";
          value_status += minute();
        }

        switch (rtc_boot_mode.twitter_phase) {

          case 0:
            // WIFI : GOPRO
            // power on gopro, picture mode change, shutter on, get file name and directory of last taken pic
            get_gpro_list();
            break;

          case 1:
            // WIFI : GOPRO
            // download last taken pic to spiffs, power off gopro
            get_gopro_file();
            gopro_poweroff();
            delay(200);
            ESP.reset();
            break;

          case 2:
            // https://dev.twitter.com/rest/reference/post/media/upload-init
            tweet_init();
            break;

          case 3:
            // https://dev.twitter.com/rest/reference/post/media/upload-append
            tweet_append();
            break;

          case 4:
            // https://dev.twitter.com/rest/reference/post/media/upload-finalize
            tweet_fin();
            break;

          case 5:
            // https://dev.twitter.com/rest/reference/get/media/upload-status
            // not used
            tweet_check();
            delay(200);
            ESP.reset();
            break;

          case 6:
            // https://dev.twitter.com/rest/reference/post/statuses/update
            tweet_status();
            break;

          case 7:
            // go to clock mode
            rtc_boot_mode.gopro_mode = false;
            saveConfig_helper();
            delay(200);
            ESP.reset();
            break;

          case 8:
            // tweet error status
            tweet_error();
            break;

          default:
            x = false;
            break;
        }
      }
    }
  }
}

void displayData() {
  lcd.setCursor(7, 2);
  lcd.print("    ");
  lcd.setCursor(7, 2);
  if ( solar_data.data1 < 1000 ) {
    lcd.print(" ");
  }
  lcd.print(solar_data.data1, 0);

  lcd.setCursor(12, 2);
  lcd.print("    ");
  lcd.setCursor(12, 2);
  lcd.print(solar_data.data2, 2);
}

void displayData2() {
  lcd.setCursor(7, 3);
  lcd.print("    ");
  lcd.setCursor(7, 3);
  if ( solar_data.data3 < 1000 ) {
    lcd.print(" ");
  }
  lcd.print(solar_data.data3, 0);

  lcd.setCursor(12, 3);
  lcd.print("    ");
  lcd.setCursor(12, 3);
  lcd.print(solar_data.data4, 5);
  //lcd.print((solar_data.data4 * 100000), 0);
}

void displaypir() {
  if ( solar_data.pir == 1) {
    for ( int i = 0 ; i <= 2 ; i ++ ) {
      lcd.setCursor(19, i);
      lcd.write(5);
    }
  } else {
    for ( int i = 0 ; i <= 2 ; i ++ ) {
      lcd.setCursor(19, i);
      lcd.print(" ");
    }
  }
}

void displaypowerAvg() {
  if (solar_data.powerAvg < 9999) {
    String str_Power = String(solar_data.powerAvg);
    int length_Power = str_Power.length();

    lcd.setCursor(2, 3);
    for ( int i = 0; i < ( 4 - length_Power ) ; i++ ) {
      lcd.print(" ");
    }
    lcd.print(str_Power);
  }
}

void displayTemperaturedigit(float Temperature) {
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(Temperature, 1);
}

void displayTemperature() {
  lcd.setCursor(1, 1);
  displayTemperaturedigit(solar_data.Temperature1);

  lcd.setCursor(7, 1);

  float tempdiff = solar_data.Temperature2 - solar_data.Temperature1 ;
  displayTemperaturedigit(solar_data.Temperature2);

  lcd.setCursor(14, 1);
  if ( tempdiff > 0 ) {
    lcd.print("+");
  } else if ( tempdiff < 0 ) {
    lcd.print("-");
  }

  String str_tempdiff = String(int abs(tempdiff));
  int length_tempdiff = str_tempdiff.length();

  lcd.setCursor(15, 1);
  lcd.print(abs(tempdiff), 1);
  if ( length_tempdiff == 1) {
    lcd.print(" ");
  }

  lcd.setCursor(2, 2);
  if ( solar_data.Humidity >= 10 ) {
    lcd.print(solar_data.Humidity, 1);
  } else {
    lcd.print(" ");
    lcd.print(solar_data.Humidity, 1);
  }
}
void digitalClockDisplay() {
  // digital clock display of the time
  lcd.setCursor(0, 0);
  printDigitsnocolon(month());
  lcd.print("/");
  printDigitsnocolon(day());

  lcd.setCursor(6, 0);
  lcd.print(dayShortStr(weekday()));
  lcd.setCursor(10, 0);
  printDigitsnocolon(hour());
  printDigits(minute());
  printDigits(second());
}

void printDigitsnocolon(int digits) {
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}


void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  lcd.print(":");
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}

/* ------------------------------- */
String get_hash_str(String content_more, String content_last, int positionofchunk, int get_size, bool bpost = false) {

  File f = SPIFFS.open("/" + gopro_file, "r");
  if (!f) {
    return "0";
  } else {
    f.seek(positionofchunk, SeekSet);
  }

  if ( !bpost ) {

    char buff[1400] = { 0 };
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

    return URLEncode(base64::encode(digestkey, 20).c_str());

  } else {

    char buff[1400] = { 0 };
    int len = get_size;

    sslclient.print(content_more);

    lcd.setCursor(0, 3);
    lcd.print("[P:3] put : ");

    int pre_progress = 0;
    int count = 0;

    while (f.available()) {
      int c = f.readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
      if ( c > 0 ) {
        sslclient.write((const uint8_t *) buff, c);
      }

      float progress = ((get_size - len) / (get_size / 100));
      if (int(progress) != pre_progress ) {
        lcd.setCursor(13, 3);
        if (progress < 10) {
          lcd.print(" ");
        }

        lcd.print(progress, 0);
        lcd.print(" % ");
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

      lcd.setCursor(0, 2);
      lcd.print("[P:3] uld : ");
      lcd.print(count);
      count++;

      // up error
      if (count > 300) {
        rtc_boot_mode.attempt_this++;
        saveConfig_helper();

        ESP.reset();
      }
      yield();
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
    lcd.setCursor(0, 2);
    lcd.print("[P:3] ssl fail");
    return false;
  }

  wifiClient.setNoDelay(true);
  sslclient.setNoDelay(true);

  sslclient.print(content_header);

  String ok = get_hash_str(content_more, content_last, positionofchunk, get_size, true);
  if (ok == "0") {
    lcd.setCursor(0, 2);
    lcd.print("[P:3] put err, reset");
    delay(2000);
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
    rtc_boot_mode.attempt_this = 0;
    saveConfig_helper();
    return true;
  } else {
    return false;
  }
}

bool do_http_text_post(String OAuth_header) {
  String httppayload = "";
  String req_body_to_post;

  String uri_to_post = UPLOAD_BASE_URI;

  if (rtc_boot_mode.twitter_phase == 6) {
    uri_to_post = BASE_URI;
    uri_to_post += "?media_ids=";
    uri_to_post += media_id;
  }

  if (rtc_boot_mode.twitter_phase == 8) {
    uri_to_post = BASE_URI;
  }

  HTTPClient http;

  lcd.setCursor(0, 1);
  lcd.print("case ");
  lcd.print(rtc_boot_mode.twitter_phase);
  lcd.print(" : ");
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

    case 8:
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

  if (httpCode > 0) {
    if (rtc_boot_mode.twitter_phase == 2 || rtc_boot_mode.twitter_phase == 4) {
      if ((httpCode >= 200) && (httpCode < 400)) {
        httppayload = http.getString();
      }
    }
    http.end();
    delay(100);

    if (rtc_boot_mode.twitter_phase == 6) {
      syslogPayload = "do_http_text_post 1: ";
      syslogPayload += " - httpCode : ";
      syslogPayload += httpCode;
      sendUdpSyslog(syslogPayload);
      delay(200);
    }

    lcd.print(httpCode);
    lcd.setCursor(0, 2);

  } else {
    http.end();
    if (rtc_boot_mode.twitter_phase == 6) {
      syslogPayload = "do_http_text_post 2: ";
      syslogPayload += " - httpCode : ";
      syslogPayload += httpCode;
      sendUdpSyslog(syslogPayload);

      rtc_boot_mode.attempt_this++;
      saveConfig_helper();
      delay(200);
    }

    if (rtc_boot_mode.twitter_phase == 2 ) {
      rtc_boot_mode.attempt_this++;
      saveConfig_helper();
      delay(200);
    }
    lcd.print(httpCode);
    delay(1000);
    return false;
  }

  delay(1000);

  if (rtc_boot_mode.twitter_phase == 6 || rtc_boot_mode.twitter_phase == 8) {
    if ((httpCode >= 200) && (httpCode < 400)) {
      rtc_boot_mode.attempt_this  = 0;
      rtc_boot_mode.twitter_phase = 7;
      rtc_boot_mode.attempt_phase = 7;
      saveConfig_helper();
      delay(200);
      return true;
    } else {
      if (rtc_boot_mode.twitter_phase == 6) {
        rtc_boot_mode.attempt_this   = 0;
        rtc_boot_mode.attempt_phase  = 6;
        rtc_boot_mode.attempt_detail = 1;
        rtc_boot_mode.twitter_phase  = 8;
        saveConfig_helper();
        delay(200);
      }
      return false;
    }
  }

  if (rtc_boot_mode.twitter_phase == 2 || rtc_boot_mode.twitter_phase == 4) {
    char json[] = "{\"media_id\":000000000000000000,\"media_id_string\":\"000000000000000000\",\"size\":0000000,\"expires_after_secs\":86400,\"image\":{\"image_type\":\"image\\/jpeg\",\"w\":0000,\"h\":0000}}" ;
    httppayload.toCharArray(json, 200);
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
        rtc_boot_mode.attempt_phase = 3;
        saveConfig_helper();
        return true;
      }
    } else {
      if (root.containsKey("media_id_string")) {
        /*
          if (media_id == root["media_id_string"].asString()) {
          if (root.containsKey("processing_info")) {
            rtc_boot_mode.attempt_this  = 0;
            rtc_boot_mode.twitter_phase = 5;
            rtc_boot_mode.attempt_phase = 5;
            saveConfig_helper();
            return true;
          } else {
            rtc_boot_mode.attempt_this  = 0;
            rtc_boot_mode.twitter_phase = 6;
            rtc_boot_mode.attempt_phase = 6;
            saveConfig_helper();
            return true;
          }
          }
        */
        rtc_boot_mode.attempt_this  = 0;
        rtc_boot_mode.twitter_phase = 5;
        rtc_boot_mode.attempt_phase = 5;
        saveConfig_helper();
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

    case 8:
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
    case 2: // INIT
      para_string += URLEncode(UPLOAD_COMMAND);
      para_string += "=" ;
      para_string += URLEncode(UPLOAD_CMD_INIT);
      para_string += "&";

      para_string += URLEncode(KEY_MEDIA_TYPE);
      para_string += "=" ;
      para_string += URLEncode(UPLOAD_MEDIA_TYPE);
      para_string += "&";
      break;

    case 3: // APPEND
      para_string += URLEncode(UPLOAD_OAUTH_KEY);
      para_string += "=" ;
      para_string += hashStr.c_str();
      para_string += "&";
      break;

    case 4: // FIN
      para_string += URLEncode(UPLOAD_COMMAND);
      para_string += "=" ;
      para_string += URLEncode(UPLOAD_CMD_FINALIZE);
      para_string += "&";

      para_string += URLEncode(KEY_MEDIA_ID);
      para_string += "=" ;
      para_string += URLEncode(media_id.c_str());
      para_string += "&";
      break;

    case 6: // UPDATE
      para_string += URLEncode(KEY_MEDIA_IDS);
      para_string += "=" ;
      para_string += URLEncode(media_id.c_str());
      para_string += "&";
      break;

    case 8: // Error report
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

    case 8:
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

/* PHASE 8 : TWEET error*/
bool tweet_error() {
  bool rtn = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:8] TWEET error");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  lcd.setCursor(0, 2);
  if (rtn) {
    lcd.print("[P:8] OK");
  } else {
    lcd.print("[P:8] FAIL");
  }
  delay(2000);

  return rtn;
}


/* PHASE 6 : TWEET */
bool tweet_status() {
  bool rtn = false;

  if ( rtc_boot_mode.attempt_this > 4) {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 8;
    saveConfig_helper();
    delay(200);
    return rtn;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:6] TWEET");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  lcd.setCursor(0, 2);
  if (rtn) {
    lcd.print("[P:6] OK");
  } else {
    lcd.print("[P:6] FAIL");
  }
  delay(2000);

  return rtn;
}

/* PHASE 5 : STATUS CHECK */
void tweet_check() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:5] CHECKING");
  rtc_boot_mode.attempt_this  = 3;
  rtc_boot_mode.twitter_phase = 6;
  saveConfig_helper();
  delay(2000);
}

/* PHASE 4 : FINALIZE */
bool tweet_fin() {
  bool rtn = false;

  if ( rtc_boot_mode.attempt_this > 4) {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 8;
    saveConfig_helper();
    delay(200);
    return rtn;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:4] FINALIZE");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  lcd.setCursor(0, 1);
  if (rtn) {
    lcd.print("[P:4] OK");
  } else {
    lcd.print("[P:4] FAIL");
  }
  delay(2000);
  return rtn;
}

/* PHASE 3 : APPEND */
bool tweet_append() {
  bool rtn = false;

  if ( rtc_boot_mode.attempt_this > 4) {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 8;
    saveConfig_helper();
    delay(200);
    return rtn;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:3] APPEND");
  lcd.setCursor(0, 1);
  lcd.print("[P:3] chk : ");
  lcd.print(rtc_boot_mode.chunked_no);

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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:3] APPEND");
  lcd.setCursor(0, 1);
  if (rtn) {
    lcd.print("[P:3] chk : ");
    lcd.print(rtc_boot_mode.chunked_no - 1);
    lcd.print(" OK");
  } else {
    lcd.print(" FAIL");
  }

  lcd.setCursor(0, 2);
  lcd.print("[P:3] ");
  lcd.print(upspeed, 0);
  lcd.print(" B");
  delay(2000);

  return rtn;
}

/* PHASE 2 : INIT */
bool tweet_init() {
  bool rtn = false;

  if ( rtc_boot_mode.attempt_this > 4) {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 8;
    saveConfig_helper();
    delay(200);
    return rtn;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:2] INIT");

  String para_string     = make_para_string();
  String base_string     = make_base_string(para_string);
  String oauth_signature = make_signature(CONSUMER_SECRET, ACCESS_SECRET, base_string);
  String OAuth_header    = make_OAuth_header(oauth_signature);
  rtn                    = do_http_text_post(OAuth_header);

  lcd.clear();
  lcd.setCursor(0, 0);
  if (rtn) {
    lcd.print("[P:2] ");
    lcd.setCursor(0, 1);
    lcd.print(media_id);
  } else {
    lcd.print("[P:2] FAIL");
  }
  delay(2000);

  return rtn;
}

bool gopro_poweroff() {
  /*
    bool rtn = false;
    HTTPClient http;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("[P:1] PN DOWN");

    // auto shutdown off
    http.begin("http://10.5.5.9:80/camera/AO?t=" + String(gopropassword) + "&p=%00");
    int httpCode = http.GET();

    lcd.setCursor(0, 1);
    lcd.print("code : ");
    lcd.print(httpCode);

    if (httpCode == HTTP_CODE_OK) {
      http.end();
      delay(2000);
      // power off
      http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%00");
      int httpCode2 = http.GET();

      lcd.setCursor(0, 2);
      lcd.print("code : ");
      lcd.print(httpCode2);

      if (httpCode2 == HTTP_CODE_OK) {
        rtn = true;
      }
    }
    http.end();
    return rtn;
  */
  return true;
}

bool gopro_poweron() {
  bool rtn = false;
  HTTPClient http;

  /*
    http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%01");
    int httpCode = http.GET();
  */
  int httpCode = 200;

  lcd.setCursor(0, 1);
  lcd.print("code : ");
  lcd.print(httpCode);


  if (httpCode == HTTP_CODE_OK) {
    http.end();
    delay(3000);
    // 5MP
    //http.begin("http://10.5.5.9:80/camera/PR?t=" + String(gopropassword) + "&p=%03");
    // 7MP WIDE
    http.begin("http://10.5.5.9:80/camera/PR?t=" + String(gopropassword) + "&p=%04");
    int httpCode2 = http.GET();

    lcd.print(" ");
    lcd.print(httpCode2);

    if (httpCode2 == HTTP_CODE_OK) {
      http.end();
      delay(1000);
      http.begin("http://10.5.5.9:80/bacpac/SH?t=" + String(gopropassword) + "&p=%01");
      int httpCode3 = http.GET();

      lcd.print(" ");
      lcd.print(httpCode3);

      if (httpCode3 == HTTP_CODE_OK) {
        rtn = true;
      }
    }
  }

  http.end();
  return rtn;
}


/* PHASE 1 : start : get last file */
bool get_gopro_file() {
  if ( rtc_boot_mode.attempt_this > 2) {
    rtc_boot_mode.attempt_this  = 0;
    rtc_boot_mode.twitter_phase = 8;
    saveConfig_helper();
    gopro_poweroff();
    delay(200);
    ESP.reset();
  }

  bool rtn = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:1] GET ");

  HTTPClient http;
  wifiClient.setNoDelay(true);
  sslclient.setNoDelay(true);

  String url = "http://10.5.5.9:8080/videos/DCIM/";
  url += gopro_dir;
  url += "/";
  url += gopro_file;

  http.begin(url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      int len = http.getSize();
      uint8_t buff[1460] = { 0 };

      if ( len != rtc_boot_mode.gopro_size ) {
        http.end();
        rtc_boot_mode.twitter_phase = 0;
        rtc_boot_mode.attempt_this++;

        lcd.setCursor(0, 1);
        lcd.print("[P:1] size differ");
        lcd.setCursor(0, 2);
        lcd.print("[P:1] go to P:0");
        delay(1000);

        saveConfig_helper();
        return false;
      }

      WiFiClient * stream = http.getStreamPtr();
      //stream->setTimeout(1000);
      File f = SPIFFS.open("/" + gopro_file, "w");
      if (!f) {
        lcd.setCursor(0, 1);
        lcd.print("[P:1] SPIFFS err");
        delay(1000);

        rtc_boot_mode.attempt_this++;
        saveConfig_helper();
        return false;
      }

      lcd.setCursor(0, 1);
      lcd.print("[P:1] dnd : ");

      int pre_progress = 0;
      int count = 0;

      unsigned long dnstart = millis();

      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          f.write(buff, c);

          float progress = ((rtc_boot_mode.gopro_size - len) / (rtc_boot_mode.gopro_size / 100));
          if (int(progress) != pre_progress ) {
            lcd.setCursor(13, 1);
            if (progress < 10) {
              lcd.print(" ");
            }

            lcd.print(progress, 0);
            lcd.print(" %");
            pre_progress = int(progress);
          }

          if (len > 0) {
            len -= c;
          }
        }
        lcd.setCursor(0, 2);
        lcd.print("[P:1] cnt : ");
        lcd.print(count);
        count++;

        // download error
        if ( count > 1800 ) {
          rtc_boot_mode.formatspiffs = true;
          rtc_boot_mode.attempt_this++;
          rtc_boot_mode.twitter_phase = 0;
          saveConfig_helper();

          return false;
        }
        yield();
      }
      f.close();

      unsigned long dnstop = millis();
      float timestook = ( dnstop - dnstart ) / 1000 ;
      float dnspeed = rtc_boot_mode.gopro_size / (( dnstop - dnstart ) / 1000) ;

      lcd.setCursor(0, 2);
      lcd.print("[P:1]              ");
      lcd.setCursor(5, 2);
      lcd.print(dnspeed, 0);
      lcd.print(" K ");
      lcd.print(timestook, 1);
      lcd.print(" S");

      lcd.setCursor(0, 3);
      lcd.print("[P:1] verify : ");


      delay(2000);
      File fr = SPIFFS.open("/" + gopro_file, "r");
      if (!fr || fr.size() != rtc_boot_mode.gopro_size ) {
        lcd.print("err");
        delay(1000);

        rtc_boot_mode.formatspiffs = true;
        rtc_boot_mode.attempt_this++;
        rtc_boot_mode.twitter_phase = 0;
        saveConfig_helper();

        fr.close();
        return false;
      }
      fr.close();

      lcd.print("OK");
      delay(3000);

      rtc_boot_mode.attempt_this  = 0;
      rtc_boot_mode.twitter_phase = 2;
      rtc_boot_mode.attempt_phase = 2;
      saveConfig_helper();

      rtn = true;
    }
  } else {
    rtn = false;
  }
  http.end();

  lcd.clear();
  lcd.setCursor(0, 0);
  if (rtn) {
    lcd.print("[P:1] OK");
  } else {
    lcd.print("[P:1] FAIL");
  }

  delay(2000);
  return rtn;
}

/* PHASE 0 : start : get gopro file list */
bool get_gpro_list() {
  if ( rtc_boot_mode.attempt_this > 4) {
    rtc_boot_mode.twitter_phase = 8;
    rtc_boot_mode.attempt_this  = 0;
    saveConfig_helper();
    gopro_poweroff();
    delay(200);
    ESP.reset();
  }

  bool rtn = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:0] POWER ON ");
  int Attempt = 0;
  while (1) {
    if (gopro_poweron()) {
      lcd.setCursor(0, 2);
      lcd.print("---> OK");
      delay(3000);

      break;
    } else {
      delay(1000);
    }

    Attempt++;
    if (Attempt > 5) {
      lcd.setCursor(0, 2);
      lcd.print("---> FAIL");
      delay(1000);

      rtc_boot_mode.attempt_this++;
      saveConfig_helper();
      return false;
    }
  }

  delay(2000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:0] LIST ");

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

        lcd.setCursor(0, 1);
        lcd.print(filesize.toInt());

        if ( filesize.toInt() < 2600000 ) {
          gopro_dir     = directory.c_str();
          gopro_file    = filename.c_str();
          media_id      = "0000000000000000000";
          rtc_boot_mode.gopro_size    = filesize.toInt();
          rtc_boot_mode.chunked_no    = 0;
          rtc_boot_mode.twitter_phase = 1;
          rtc_boot_mode.attempt_phase = 1;
          rtc_boot_mode.attempt_this  = 0;
          rtc_boot_mode.pic_taken     = minute();

          saveConfig_helper();

          rtn = true;
        } else {

          lcd.setCursor(0, 2);
          lcd.print("[P:0] big file");
          delay(2000);

          rtc_boot_mode.attempt_this++;
          saveConfig_helper();
          rtn = false;
        }
      }
    }
  } else {
    rtn = false;
  }
  http.end();

  //lcd.clear();
  lcd.setCursor(0, 3);
  if (rtn) {
    lcd.print("[P:0] ");
    lcd.print(gopro_file);
  } else {
    lcd.print("[P:0] FAIL");
  }
  delay(2000);

  return rtn;
}

// end

