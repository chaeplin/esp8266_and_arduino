// based on tweeting_silicon : http://harizanov.com/2015/06/tweeting-silicon/
// https://github.com/igrr/axtls-8266/blob/master/crypto/hmac.c
// http://hardwarefun.com/tutorials/url-encoding-in-arduino
// chaeplin
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <base64.h>
#include "FS.h"

#define SHA1_SIZE 20

extern "C" {
  typedef struct
  {
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

#include "/usr/local/src/ap_setting.h"
#include "/usr/local/src/twitter_setting.h"

#define INFO_PRINT 0
#define DEBUG_PRINT 0

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

IPAddress time_server = MQTT_SERVER;

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

// for image upload
const char* upload_filename = "/0416.jpg";

const char upload_base_host[]    = "upload.twitter.com";
const char upload_base_url[]     = "https://upload.twitter.com/1.1/media/upload.json";
const char upload_base_uri[]     = "/1.1/media/upload.json";
const char upload_fingerprint[]  = "95 00 10 59 C8 27 FD 2C D0 76 12 F7 88 35 64 21 F5 60 D3 E9";
const char upload_oauth_key[]    = "oauth_body_hash";

const char* imagearray[5] = { upload_base_host, upload_base_url, upload_base_uri, upload_fingerprint, upload_oauth_key };

//WiFiClient client;
WiFiClientSecure client;
//client.setNoDelay(1);
WiFiUDP udp;

unsigned int localPort = 12390;
const int timeZone     = 9;

const char* filename = "/tweet_done.txt";
bool x;
int filesize;

void wifi_connect() {
  Serial.print("[WIFI] start millis     : ");
  Serial.println(millis());
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
  Serial.print("[WIFI] connected millis : ");
  Serial.print(millis());
  Serial.print(" - ");
  Serial.println(WiFi.localIP());
}

void setup() {
  system_update_cpu_freq(SYS_CPU_80MHz);

  Serial.begin(115200);
  Serial.println();

  wifi_connect();

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    setSyncProvider(getNtpTime);
  }

  print_FreeHeap();

  filesize = 0;
  x = false;

  bool result = SPIFFS.begin();
  Serial.println();
  Serial.println(" ");
  Serial.printf("[SPIFFS] opened: %d\n", result);
  if (!result) {
    Serial.println("[SPIFFS] SPIFFS has problem");
  } else {
    File f = SPIFFS.open(filename, "r");
    if (!f) {
      Serial.println("[SPIFFS] do not tweet");
      x = false;
    } else {
      Serial.println("[SPIFFS] do tweet");
      File f = SPIFFS.open(filename, "w");
      f.println("done");
      x = true;
    }
    f.close();

    print_FreeHeap();

    File fu = SPIFFS.open(upload_filename, "r");
    if (!fu) {
      Serial.println("[SPIFFS] no file");
    } else {
      filesize = fu.size();
      Serial.println("[SPIFFS] image to tweet");
      Serial.printf("[SPIFFS] file.name(): %s\n", fu.name());
      Serial.printf("[SPIFFS] file.size(): %d\n", fu.size());
    }
    f.close();
  }
  print_FreeHeap();
}

time_t prevDisplay = 0;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      if (timeStatus() == timeSet) {        
        digitalClockDisplay();
        print_FreeHeap();

        if (x) {
          do_tweet();
          x = false;
        }

      }
    }
  } else {
    wifi_connect();
    setSyncProvider(getNtpTime);
  }
}

void do_tweet() {
  print_FreeHeap();
  const char* value_status  = "esp-01 + direct tweet / test 5";

  String media_id_to_associate = tweeting(value_status, upload_filename, "0");
  Serial.print("[RESULT] : IMAGE UPLOAD : ");
  Serial.println(media_id_to_associate);
  if ( media_id_to_associate != "0" ) {
    media_id_to_associate = tweeting(value_status, upload_filename, media_id_to_associate);
    Serial.print("[RESULT] : TEXT : ");
    Serial.println(media_id_to_associate);
  }
}

void print_FreeHeap() {
  Serial.print("[HEAP] mills : ");
  Serial.print(millis());
  Serial.print(" - heap : ");
  Serial.println(ESP.getFreeHeap());
  Serial.flush();
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

    int content_length = filesize + content_more.length() + content_last.length();

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

  File f = SPIFFS.open(image_file_name, "r");
  if (!f) {
    Serial.println("[SPIFFS] File doesn't exist yet");
    return "0";
  }

  print_FreeHeap();
  if (body_hash) {
    char buff[1024] = { 0 };
    int len = filesize;

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

    return URLEncode(base64::encode(digestkey, 20).c_str());

  } else {

    char buff[1024] = { 0 };
    int len = filesize;

    client.print(content_more);

    while (f.available()) {
      int c = f.readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
      client.write((uint8_t *)buff, c);
      if (len > 0) {
        len -= c;
      }
    }
    client.print(content_last);
    f.close();
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

  File f = SPIFFS.open(image_file_name, "r");
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

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(line);

  print_FreeHeap();
  if (!json.success()) {
    Serial.println("Failed to parse config file");
    return "0";
  }

  if ( json.containsKey("media_id_string") ) {
    return json["media_id_string"];
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
