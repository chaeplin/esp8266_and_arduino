// esp-01 4M / 3M flash / esp-solar
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
#define DEBUG_PRINT 1

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

#define SquareWavePin 1
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

//time_t getNtpTime();
//String macToStr(const uint8_t* mac);
void callback(char* intopic, byte* inpayload, unsigned int length);
void sendmqttMsg(char* topictosend, String payload);
void sendNTPpacket(IPAddress & address);
void sendUdpSyslog(String msgtosend);
void sendUdpmsg(String msgtosend);

/* ---- */
const char* ssid          = WIFI_SSID;
const char* password      = WIFI_PASSWORD;
const char* goprossid     = GOPRO_SSID;
const char* gopropassword = GOPRO_PASSWORD;
const char* otapassword   = OTA_PASSWORD;
/* ---- */

//
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

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

//
#define RTC_MAGIC 1667678

struct {
  uint32_t magic;
  bool gopro_mode;
  bool formatspiffs;
  float Temperature;
  // more on gopro attempt
  //
} rtc_boot_mode;
//

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
/* ---- */
uint32_t value_timestamp;
uint32_t value_nonce;
/* ---- */
String value_status;
/* ---- */
//
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

void alm_isr() {
  balm_isr = true;
}

void rtc_boot_check() {
  system_rtc_mem_read(100, &rtc_boot_mode, sizeof(rtc_boot_mode));
  if (rtc_boot_mode.magic != RTC_MAGIC) {
    rtc_boot_mode.magic        = RTC_MAGIC;
    rtc_boot_mode.gopro_mode   = false;
    rtc_boot_mode.formatspiffs = false;
    rtc_boot_mode.Temperature  = 0;
  }
}

bool saveConfig() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  json["goproDir"]     = gopro_dir;
  json["goproFile"]    = gopro_file;
  json["goproSize"]    = gopro_size;
  json["twitterPhase"] = twitter_phase;
  json["mediaId"]      = media_id;
  json["chunkedNo"]    = chunked_no;
  json["attemptThis"]  = attempt_this;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    return false;
  }

  json.printTo(configFile);
  return true;
}

bool loadConfig() {
  File configFile = SPIFFS.open("/config.json", "r");
  if (!configFile) {
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if (!json.success()) {
    return false;
  }

  gopro_dir     = json["goproDir"].asString();
  gopro_file    = json["goproFile"].asString();
  gopro_size    = json["goproSize"];
  twitter_phase = json["twitterPhase"];
  media_id      = json["mediaId"].asString();
  chunked_no    = json["chunkedNo"];
  attempt_this  = json["attemptThis"];

  return true;
}

void saveConfig_helper() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (!saveConfig()) {
    lcd.print("[CONFIG] save fail");
  } else {
    lcd.print("[CONFIG] saved");
  }
  delay(2000);
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
      ESP.restart();
    }
  }
  lcd.setCursor(0, 2);
  lcd.print("ip: ");
  lcd.print(WiFi.localIP());
  delay(1000);
}

void wifi_connect() {
  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
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
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

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
      /*
        if (mqttclient.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        mqttclient.publish(willTopic, "1", true);

        if (!ResetInfo) {
          mqttclient.publish(hellotopic, (char*) getResetInfo.c_str());
          ResetInfo = true;
        } else {
          mqttclient.publish(hellotopic, "hello again 1 from solar");
        }

      */

      mqttclient.loop();

      for (int i = 0; i < 6; ++i) {
        mqttclient.subscribe(substopic[i]);
        mqttclient.loop();
      }

      if (DEBUG_PRINT) {
        sendUdpSyslog("---> mqttconnected");
      }
    } else {
      if (DEBUG_PRINT) {
        syslogPayload = "failed, rc=";
        syslogPayload += mqttclient.state();
        sendUdpSyslog(syslogPayload);
      }
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

  if (DEBUG_PRINT) {
    syslogPayload = intopic;
    syslogPayload += " ====> ";
    syslogPayload += receivedpayload;
    sendUdpSyslog(syslogPayload);
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
  system_update_cpu_freq(SYS_CPU_80MHz);
  WiFi.mode(WIFI_OFF);
  rtc_boot_check();

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
    saveConfig_helper();
    rtc_boot_mode.formatspiffs = false;
    system_rtc_mem_write(100, &rtc_boot_mode, sizeof(rtc_boot_mode));
  }

  if (rtc_boot_mode.gopro_mode) {
    lcd.setCursor(0, 1);
    if (!loadConfig()) {
      lcd.print("[CONFIG] load fail");
    } else {
      lcd.print("[CONFIG] loaded");
      lcd.setCursor(0, 2);
      lcd.print("[CONFIG] [PH] : ");
      lcd.print(twitter_phase);
    }
    delay(1000);

    // check fie size in config
    if ( twitter_phase != 0 && gopro_size == 0) {
      attempt_this  = 0;
      twitter_phase = 0;
      saveConfig_helper();
      delay(200);
      ESP.reset();
    }

    if ( twitter_phase == 0) {
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        if ( dir.fileName().startsWith("/GOPR")) {
          SPIFFS.remove(dir.fileName());
        }
      }
    }

  }

  if (!rtc_boot_mode.gopro_mode) {
    wifi_connect();
  } else {
    if ( twitter_phase > 1 ) {
      wifi_connect();
    } else {
      gopro_connect();
    }
  }

  if (!rtc_boot_mode.gopro_mode || twitter_phase > 1) {
    udp.begin(localPort);

    if (!Rtc.IsDateTimeValid()) {
      if (DEBUG_PRINT) {
        sendUdpSyslog("00 --> Rtc.IsDateTime is inValid");
      }

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
        if (DEBUG_PRINT) {
          sendUdpSyslog("00 --> ntp time synced");
        }

        lcd.setCursor(0, 3);
        lcd.print("ntp synced");

        Rtc.SetDateTime(now() - 946684800);
        if (!Rtc.GetIsRunning()) {
          Rtc.SetIsRunning(true);
        }
      } else {
        if (DEBUG_PRINT) {
          sendUdpSyslog("00 --> ntp not synced, use rtc time anyway");
        }
      }
    } else {
      if (DEBUG_PRINT) {
        sendUdpSyslog("00 --> Rtc.IsDateTime is Valid");
      }
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

    if (DEBUG_PRINT) {
      syslogPayload = "------------------> solar started";
      sendUdpSyslog(syslogPayload);
    }

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
      /*
        if (flag & DS3231AlarmFlag_Alarm1) {
        if (DEBUG_PRINT) {
          sendUdpSyslog("alarm one triggered");
        }
        }
      */
      if (flag & DS3231AlarmFlag_Alarm2) {
        if (DEBUG_PRINT) {
          sendUdpSyslog("alarm two triggered");
        }
        rtc_boot_mode.gopro_mode  = true;
        rtc_boot_mode.Temperature = solar_data.Temperature2 ;
        system_rtc_mem_write(100, &rtc_boot_mode, sizeof(rtc_boot_mode));

        attempt_this  = 0;
        twitter_phase = 0;
        saveConfig_helper();
        delay(200);

        ESP.reset();
      }
      balm_isr = false;
    }

    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttclient.connected()) {
        if (DEBUG_PRINT) {
          syslogPayload = "failed, rc= ";
          syslogPayload += mqttclient.state();
          sendUdpSyslog(syslogPayload);
        }
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
                lcd.setCursor(19, 2);
                lcd.write(5);
              } else {
                lcd.setCursor(19, 2);
                lcd.print(" ");
              }

              if (( millis() - lastTime2 ) > 120000 ) {
                lcd.setCursor(19, 3);
                lcd.write(5);
              } else {
                lcd.setCursor(19, 3);
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

        value_status  = "esp-01 / ";
        value_status += rtc_boot_mode.Temperature;
        value_status += "C / ";
        value_status += hour();
        value_status += ":";
        value_status += minute();

        switch (twitter_phase) {

          case 0:
            get_gpro_list();
            break;

          case 1:
            if (get_gopro_file()) {
              gopro_poweroff();
              delay(200);
              ESP.reset();
            }
            break;

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
            system_rtc_mem_write(100, &rtc_boot_mode, sizeof(rtc_boot_mode));
            delay(200);
            ESP.reset();
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

void sendmqttMsg(char* topictosend, String payload) {
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (mqttclient.publish(topictosend, p, msg_length, 1)) {
    free(p);
  } else {
    if (DEBUG_PRINT) {
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish fail";
      sendUdpSyslog(syslogPayload);
    }
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

/* ------------------------------- */
String get_hash_str(String content_more, String content_last, int positionofchunk, int get_size, bool bpost = false) {

  File f = SPIFFS.open("/" + gopro_file, "r");
  if (!f) {
    return "0";
  } else {
    f.seek(positionofchunk, SeekSet);
  }

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

    return URLEncode(base64::encode(digestkey, 20).c_str());
  } else {

    char buff[1460] = { 0 };
    int len = get_size;

    sslclient.print(content_more);

    lcd.setCursor(0, 3);
    lcd.print("[P:3] put : ");

    int pre_progress = 0;

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
        return "0";
        break;
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
    lcd.setCursor(0, 2);
    lcd.print("[P:3] ssl fail");
    return false;
  }

  wifiClient.setNoDelay(true);
  sslclient.setNoDelay(true);

  sslclient.print(content_header);

  get_hash_str(content_more, content_last, positionofchunk, get_size, true);

  int _returnCode = 0;
  while (sslclient.connected()) {
    String line = sslclient.readStringUntil('\n');
    if (line.startsWith("HTTP/1.")) {
      _returnCode = line.substring(9, line.indexOf(' ', 9)).toInt();
      break;
    }
  }

  if (_returnCode >= 200 && _returnCode < 400) {
    chunked_no++;
    saveConfig_helper();
    return true;
  } else {
    return false;
  }
}

bool do_http_text_post(String OAuth_header) {
  String payload = "";

  String uri_to_post = UPLOAD_BASE_URI;
  if (twitter_phase == 6) {
    uri_to_post = BASE_URI;
    uri_to_post += "?media_ids=";
    uri_to_post += media_id;
  }

  String req_body_to_post;

  HTTPClient http;

  lcd.setCursor(0, 1);
  lcd.print("case ");
  lcd.print(twitter_phase);
  lcd.print(" : ");
  switch (twitter_phase) {
    case 2:
      req_body_to_post = "command=INIT&media_type=image%2Fjpeg&total_bytes=";
      req_body_to_post += gopro_size;
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
  lcd.print(httpCode);
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

  if (twitter_phase == 6) {
    if (DEBUG_PRINT) {
      syslogPayload = "twitter : 6 - ";
      syslogPayload += httpCode;
      sendUdpSyslog(syslogPayload);
    }

    if (httpCode >= 200 && httpCode < 400) {
      attempt_this  = 0;
      twitter_phase = 7;
      saveConfig_helper();
      delay(200);
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
      para_string += URLEncode(value_status.c_str());
      break;

    default:
      break;
  }

  return para_string;
}

bool tweet_status() {
  bool rtn = false;

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

void tweet_check() {
  twitter_phase++;
}

bool tweet_fin() {
  bool rtn = false;

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

bool tweet_append() {
  bool rtn = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:3] APPEND");
  lcd.setCursor(0, 1);
  lcd.print("[P:3] chk : ");
  lcd.print(chunked_no);

  int get_size = CHUNKED_FILE_SIZE;
  int positionofchunk = (CHUNKED_FILE_SIZE * chunked_no);

  if (positionofchunk >= gopro_size) {
    twitter_phase = 4;
    saveConfig_helper();
    return true;
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
    lcd.print(chunked_no - 1);
    lcd.print(" OK");
  } else {
    lcd.print(" FAIL");
  }

  lcd.setCursor(0, 2);
  lcd.print("[P:3] ");
  lcd.print(upspeed, 0);
  lcd.print(" KB");
  delay(2000);

  return rtn;
}

bool tweet_init() {
  bool rtn = false;
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
  bool rtn = false;
  HTTPClient http;
  http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%00");
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    rtn = true;
  } else {
    rtn = false;
  }
  http.end();
  return rtn;
}

bool gopro_poweron() {
  bool rtn = false;
  HTTPClient http;

  http.begin("http://10.5.5.9:80/bacpac/PW?t=" + String(gopropassword) + "&p=%01");
  int httpCode = http.GET();

  lcd.setCursor(0, 1);
  lcd.print("code : ");
  lcd.print(httpCode);


  if (httpCode == HTTP_CODE_OK) {
    http.end();
    delay(3000);
    http.begin("http://10.5.5.9:80/camera/PR?t=" + String(gopropassword) + "&p=%03");
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

      if ( len != gopro_size ) {
        http.end();
        twitter_phase = 0;

        lcd.setCursor(0, 1);
        lcd.print("[P:1] size differ");
        lcd.setCursor(0, 2);
        lcd.print("[P:1] go to P:0");
        delay(1000);

        saveConfig_helper();
        return false;
      }

      WiFiClient * stream = http.getStreamPtr();
      File f = SPIFFS.open("/" + gopro_file, "w");
      if (!f) {
        lcd.setCursor(0, 1);
        lcd.print("[P:1] SPIFFS err");
        delay(1000);

        return false;
      }

      lcd.setCursor(0, 1);
      lcd.print("[P:1] get : ");

      int pre_progress = 0;
      unsigned long dnstart = millis();
      
      while (http.connected() && (len > 0 || len == -1)) {
        size_t size = stream->available();
        if (size) {
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          f.write(buff, c);

          float progress = ((gopro_size - len) / (gopro_size / 100));
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
      }
      f.close();
  
      unsigned long dnstop = millis();
      float timestook = ( dnstop - dnstart ) / 1000 ;
      float dnspeed = gopro_size / (( dnstop - dnstart ) / 1000) ;

      lcd.setCursor(0, 2);
      lcd.print("[P:1] ");
      lcd.print(dnspeed, 0);
      lcd.print(" K ");
      lcd.print(timestook, 1);
      lcd.print(" S");
      
      lcd.setCursor(0, 3);
      lcd.print("[P:1] verify : ");

      int Attempt = 0;
      while (!SPIFFS.open(String("/") + gopro_file, "r")) {
        delay(1000);
        Attempt++;
        if (Attempt == 5) {

          lcd.print("fail");
          delay(2000);

          return false;
        }
      }

      lcd.print("OK");
      delay(3000);

      attempt_this  = 0;
      twitter_phase = 2;
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
  bool rtn = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("[P:0] POWER ON ");
  int Attempt = 0;
  while (1) {
    if (gopro_poweron()) {
      lcd.setCursor(0, 2);
      lcd.print("---> OK");
      delay(1000);

      break;
    } else {
      delay(1000);
    }

    Attempt++;
    if (Attempt > 5) {
      lcd.setCursor(0, 2);
      lcd.print("---> FAIL");
      delay(1000);

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
          stream->setTimeout(50);
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

        if ( filesize.toInt() < 2500000 ) {
          gopro_dir     = directory.c_str();
          gopro_file    = filename.c_str();
          gopro_size    = filesize.toInt();
          media_id      = "000000000000000000";
          attempt_this  = 0;
          chunked_no    = 0;
          twitter_phase = 1;

          saveConfig_helper();

          rtn = true;
        } else {

          lcd.setCursor(0, 2);
          lcd.print("[P:0] big file");
          delay(2000);

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

