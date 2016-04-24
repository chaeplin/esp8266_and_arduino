#include <Arduino.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
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
IPAddress time_server = MQTT_SERVER;

/* -- */
// twitter
const char consumer_key[]    = ConsumerKey;
const char consumer_secret[] = ConsumerSecret;
const char access_token[]    = AccessToken;
const char access_secret[]   = AccessSecret;

const char base_host[]        = "api.twitter.com";
const char base_url[]         = "https://api.twitter.com/1.1/statuses/update.json";
const char base_uri[]         = "/1.1/statuses/update.json";
const int httpsPort           = 443;

const char* tweeter[6] = { consumer_key, consumer_secret, access_token, access_secret, base_host, base_url };

const char key_http_method[]        = "POST";
const char key_consumer_key[]       = "oauth_consumer_key";
const char key_nonce[]              = "oauth_nonce";
const char key_signature_method[]   = "oauth_signature_method";
const char key_timestamp[]          = "oauth_timestamp";
const char key_token[]              = "oauth_token";
const char key_version[]            = "oauth_version";
const char key_status[]             = "status";
const char key_signature[]          = "oauth_signature";
const char value_signature_method[] = "HMAC-SHA1";
const char value_version[]          = "1.0";
const char key_media_ids[]          = "media_ids";

const char* keys[12] = {key_http_method, key_consumer_key, key_nonce, key_signature_method, key_timestamp, key_token, key_version, key_status, key_signature, value_signature_method, value_version, key_media_ids};

const char* fingerprint = "D8 01 5B F4 6D FB 91 C6 E4 B1 B6 AB 9A 72 C1 68 93 3D C2 D9";

const char upload_base_host[]    = "upload.twitter.com";
const char upload_base_url[]     = "https://upload.twitter.com/1.1/media/upload.json";
const char upload_base_uri[]     = "/1.1/media/upload.json";
const char upload_fingerprint[]  = "95 00 10 59 C8 27 FD 2C D0 76 12 F7 88 35 64 21 F5 60 D3 E9";
const char upload_oauth_key[]    = "oauth_body_hash";

const char* imagearray[5] = { upload_base_host, upload_base_url, upload_base_uri, upload_fingerprint, upload_oauth_key };

/* -- */
//WiFiClient client;
WiFiClientSecure client;
WiFiUDP udp;

unsigned int localPort = 12390;
const int timeZone     = 9;

bool x;

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
  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
}

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
  } else if ( twitter_done == 2 ) {

    wifi_connect();
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
    }
    udp.begin(localPort);

    setSyncProvider(getNtpTime);

    if (timeStatus() == timeNotSet) {
      setSyncProvider(getNtpTime);
    }
  }

  print_FreeHeap();
  x = true;
  Serial.flush();
}

time_t prevDisplay = 0;

void loop() {

  if (WiFi.status() == WL_CONNECTED) {

    if (now() != prevDisplay) {
      prevDisplay = now();
      if (timeStatus() == timeSet) {
        digitalClockDisplay();
        print_FreeHeap();

        if (twitter_done == 2 && x) {
          do_tweet(gopro_file);
          x = false;
        }
      }
    }

    if (twitter_done == 0) {
      get_gpro_list();
    }

    if (twitter_done == 1) {
      get_gopro_file();
      delay(1000);
      ESP.reset();
    }

  } else {
    //wifi_connect();
    //setSyncProvider(getNtpTime);
  }
  print_FreeHeap();
  delay(1000);
}

void print_FreeHeap() {
  Serial.print("[HEAP] mills : ");
  Serial.print(millis());
  Serial.print(" - heap : ");
  Serial.println(ESP.getFreeHeap());
  Serial.flush();
}

void do_tweet(String upload_filename) {
  print_FreeHeap();
  const char* value_status  = "esp-01 / gopro image dn / test...";

  String media_id_to_associate = tweeting(value_status, upload_filename, "0");
  Serial.print("[RESULT] : IMAGE UPLOAD : ");
  Serial.println(media_id_to_associate);

  if ( media_id_to_associate != "0" ) {
    media_id_to_associate = tweeting(value_status, upload_filename, media_id_to_associate);
    Serial.print("[RESULT] : TEXT : ");
    Serial.println(media_id_to_associate);
  }
}

String tweeting(const char* value_status, String image_file_name, String media_id_to_associate) {
  bool image = false;
  if (media_id_to_associate == "0" ) {
    Serial.println("[TWEET] : IMAGE");
    image = true;
  } else {
    Serial.println("[TWEET] : TEXT");
  }

  uint32_t value_timestamp  = now();
  uint32_t value_nonce      = *(volatile uint32_t *)0x3FF20E44;

  String status_all = make_status_all(value_status);

  print_FreeHeap();
  if (image) {
    String content_more = "--00Twurl15632260651985405lruwT99\r\n" ;
    content_more += "Content-Disposition: form-data; name=\"media\"; filename=\"" + String(value_timestamp) + ".jpg\"\r\n" ;
    content_more += "Content-Type: application/octet-stream\r\n\r\n";

    String content_last = "\r\n--00Twurl15632260651985405lruwT99--\r\n";

    int content_length = gopro_size + content_more.length() + content_last.length();

    String hashStr         = get_spiffs_hash_post(image_file_name, true, content_more, content_last);
    String para_string     = make_para_string(status_all, value_nonce, value_timestamp, hashStr, "0");
    String base_string     = make_base_string(para_string, hashStr);
    String oauth_signature = make_signature(tweeter[1], tweeter[3], base_string);
    String OAuth_header    = make_OAuth_header(oauth_signature, value_nonce, value_timestamp, hashStr);

    Serial.print("[Oauth] para_string : ");
    Serial.println(para_string);
    Serial.print("[Oauth] base_string : ");
    Serial.println(base_string);
    Serial.print("[Oauth] hashStr : ");
    Serial.println(hashStr);
    Serial.print("[Oauth] oauth_signature : ");
    Serial.println(oauth_signature);
    Serial.print("[Oauth] OAuth_header : ");
    Serial.println(OAuth_header);

    String content_header = "POST " + String(imagearray[2]) + " HTTP/1.1\r\n";
    content_header += "Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0\r\n";
    content_header += "User-Agent: esp8266_image_bot_by_chaeplin_V_0.1\r\n";
    content_header += "Content-Type: multipart/form-data, boundary=\"00Twurl15632260651985405lruwT99\"\r\n";
    content_header += "Authorization: " + OAuth_header + "\r\n";
    content_header += "Connection: close\r\n";
    content_header += "Host: " + String(imagearray[0]) + "\r\n";
    content_header += "Content-Length: " + String(content_length) + "\r\n\r\n";

    Serial.println("[HEADER] content_header : ");
    Serial.println(content_header);

    Serial.println("[HEADER] content_more : ");
    Serial.println(content_more);

    Serial.println("[HEADER] content_last : ");
    Serial.println(content_last);

    String media_id =  do_http_image_post(tweeter[4], OAuth_header, image_file_name, content_header, content_more, content_last);
    print_FreeHeap();
    return media_id;

  } else {
    String para_string     = make_para_string(status_all, value_nonce, value_timestamp, "", media_id_to_associate);
    String base_string     = make_base_string(para_string, "");
    String oauth_signature = make_signature(tweeter[1], tweeter[3], base_string);
    String OAuth_header    = make_OAuth_header(oauth_signature, value_nonce, value_timestamp, "");

    Serial.print("[Oauth] request_body : ");
    Serial.println(status_all);
    Serial.print("[Oauth] para_string : ");
    Serial.println(para_string);
    Serial.print("[Oauth] base_string : ");
    Serial.println(base_string);
    Serial.print("[Oauth] oauth_signature : ");
    Serial.println(oauth_signature);
    Serial.print("[Oauth] OAuth_header : ");
    Serial.println(OAuth_header);

    String result = do_http_text_post(tweeter[4], OAuth_header, status_all, media_id_to_associate);
    print_FreeHeap();
    return result;
  }
}

String get_spiffs_hash_post(String image_file_name, bool body_hash, String content_more, String content_last) {

  File f = SPIFFS.open("/" + image_file_name, "r");
  if (!f) {
    Serial.println("[SPIFFS] File doesn't exist yet");
    return "0";
  }

  print_FreeHeap();
  if (body_hash) {
    Serial.println("[HASH] Start body hashing");
    digitalClockDisplay();
    print_FreeHeap();

    char buff[1460] = { 0 };
    int len = gopro_size;

    uint8_t digestkey[32];
    SHA1_CTX context;
    SHA1_Init(&context);

    SHA1_Update(&context, (uint8_t*) content_more.c_str(), content_more.length());
    while (f.available()) {
      int c = f.readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
      SHA1_Update(&context, (uint8_t*) buff, c);

      if (len > 0) {
        len -= c;
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

    uint8_t buff[1460] = { 0 };
    int len = gopro_size;

    digitalClockDisplay();

    client.print(content_more);
    while (f.available()) {
      Serial.print("  * ");

      int c = f.readBytes((char *)buff, ((len > sizeof(buff)) ? sizeof(buff) : len));

      Serial.print("  ** ");

      client.write((const uint8_t *) &buff[0], c);

      digitalClockDisplay();

      Serial.print("  ********************* : ");
      Serial.println( gopro_size - len );

      if (len > 0) {
        len -= c;
      }

      if (!client.connected()) {
        break;
      }

    }
    client.print(content_last);
    f.close();

    digitalClockDisplay();

    print_FreeHeap();
    return "1";
  }
}

String make_status_all(const char* value_status) {
  String status_all = keys[7];
  status_all += "=";
  status_all += URLEncode(value_status);
  return status_all;
}

String make_para_string(String status_all, uint32_t value_nonce, uint32_t value_timestamp, String hashStr, String media_id_to_associate) {
  String para_string;
  if (media_id_to_associate != "0") {
    para_string += keys[11];
    para_string += "=" ;
    para_string += media_id_to_associate;
    para_string += "&";
  }

  if (hashStr.length() > 0 ) {
    para_string += imagearray[4];
    para_string += "=" ;
    para_string += hashStr.c_str();
    para_string += "&";
  }

  para_string += keys[1];
  para_string += "=" ;
  para_string += tweeter[0];
  para_string += "&";
  para_string += keys[2];
  para_string += "=";
  para_string += value_nonce;
  para_string += "&";
  para_string += keys[3];
  para_string += "=";
  para_string += keys[9];
  para_string += "&";
  para_string += keys[4];
  para_string += "=";
  para_string += value_timestamp;
  para_string += "&";
  para_string += keys[5];
  para_string += "=";
  para_string += tweeter[2];
  para_string += "&";
  para_string += keys[6];
  para_string += "=";
  para_string += value_version;
  if (hashStr.length() == 0) {
    para_string += "&";
    para_string += status_all;
  }
  return para_string;
}

String make_base_string(String para_string, String hashStr) {

  if (hashStr.length() > 0) {

    String base_string = keys[0];
    base_string += "&";
    base_string += URLEncode(imagearray[1]);
    base_string += "&";
    base_string += URLEncode(para_string.c_str());
    return base_string;
  }

  String base_string = keys[0];
  base_string += "&";
  base_string += URLEncode(tweeter[5]);
  base_string += "&";
  base_string += URLEncode(para_string.c_str());
  return base_string;
}

String make_OAuth_header(String oauth_signature, uint32_t value_nonce, uint32_t value_timestamp, String hashStr) {
  String OAuth_header = "OAuth ";
  if (hashStr.length() > 0) {
    OAuth_header += imagearray[4];
    OAuth_header += "=\"";
    OAuth_header += hashStr;
    OAuth_header += "\", ";
  }

  OAuth_header += keys[1];
  OAuth_header += "=\"";
  OAuth_header += tweeter[0];
  OAuth_header += "\", ";
  OAuth_header += keys[2];
  OAuth_header += "=\"";
  OAuth_header += value_nonce;
  OAuth_header += "\", ";
  OAuth_header += keys[8];
  OAuth_header += "=\"";
  OAuth_header += oauth_signature;
  OAuth_header += "\", ";
  OAuth_header += keys[3];
  OAuth_header += "=\"";
  OAuth_header += keys[9];
  OAuth_header += "\", ";
  OAuth_header += keys[4];
  OAuth_header += "=\"";
  OAuth_header += value_timestamp;
  OAuth_header += "\", ";
  OAuth_header += keys[5];
  OAuth_header += "=\"";
  OAuth_header += tweeter[2];
  OAuth_header += "\", ";
  OAuth_header += keys[6];
  OAuth_header += "=\"";
  OAuth_header += keys[10];
  OAuth_header += "\"";
  return OAuth_header;
}

String do_http_image_post(const char* basehost, String OAuth_header, String image_file_name, String content_header, String content_more, String content_last) {

  print_FreeHeap();
  Serial.print("[HTTP] start millis     : ");
  Serial.println(millis());

  File f = SPIFFS.open("/" + image_file_name, "r");
  if (!f) {
    Serial.println("[SPIFFS] File doesn't exist yet");
    return "0";
  }

  Serial.print("connecting to ");
  Serial.println(imagearray[0]);
  if (!client.connect(imagearray[0], httpsPort)) {
    Serial.println("connection failed");
    return "0";
  }
  client.setNoDelay(true);


  Serial.print("[HTTP]  millis     : ");
  Serial.println(millis());

  if (client.verify(imagearray[3], imagearray[0])) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  Serial.print("[HTTP]  millis     : ");
  Serial.println(millis());

  Serial.print("requesting URL: ");
  Serial.println(imagearray[2]);

  Serial.print("[HTTP]  millis     : ");
  Serial.println(millis());

  print_FreeHeap();
  client.print(content_header);

  get_spiffs_hash_post(image_file_name, false, content_more, content_last);

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
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
  // {"media_id":722444351029190657,"media_id_string":"722444351029190657","size":24737,"expires_after_secs":86400,"image":{"image_type":"image\/jpeg","w":1000,"h":1335}}
  // {"media_id":723228560639447040,"media_id_string":"723228560639447040","size":1993183,"expires_after_secs":86400,"image":{"image_type":"image\/jpeg","w":3000,"h":2250}}
  //String line = "{\"media_id\":723228560639447040,\"media_id_string\":\"723228560639447040\",\"size\":1993183,\"expires_after_secs\":86400,\"image\":{\"image_type\":\"image\\/jpeg\",\"w\":3000,\"h\":2250}}" ;

  char json[] = "{\"media_id\":723228560639447040,\"media_id_string\":\"723228560639447040\",\"size\":1993183,\"expires_after_secs\":86400,\"image\":{\"image_type\":\"image\\/jpeg\",\"w\":3000,\"h\":2250}}" ;

  line.toCharArray(json, 200);
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  print_FreeHeap();
  if (!root.success()) {
    Serial.println("Failed to parse config file");
    return "0";
  }

  if ( root.containsKey("media_id_string") ) {
    return root["media_id_string"];
  }
  return "0";
}


String do_http_text_post(const char* basehost, String OAuth_header, String status_all, String media_id_to_associate) {

  String uri_to_post = base_uri;
  uri_to_post += "?media_ids=";
  uri_to_post += media_id_to_associate;

  Serial.print("[HTTP] uri : ");
  Serial.println(uri_to_post);

  print_FreeHeap();
  HTTPClient http;
  //http.begin(basehost, httpsPort, base_uri, fingerprint);
  http.begin(basehost, httpsPort, uri_to_post, fingerprint);
  http.addHeader("Authorization", OAuth_header);
  http.addHeader("Content-Length", String(status_all.length()));
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int httpCode = http.POST(status_all);
  if (httpCode > 0) {
    Serial.printf("[HTTP] POST... code: %d\n", httpCode);
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);
    }
    http.end();

    twitter_done = 3;

    if (!saveConfig()) {
      Serial.println("[CONFIG] Failed to save config");
    } else {
      Serial.println("[CONFIG] Config saved");
    }

    return String(httpCode);
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return String(httpCode);
  }
}


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
  //Serial.println(oauth_signature);

  return oauth_signature;
}

void get_gopro_file() {
  HTTPClient http;
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
      uint8_t buff[1460] = { 0 };

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

      while (!SPIFFS.open(String("/") + gopro_file, "r")) {
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
// end
