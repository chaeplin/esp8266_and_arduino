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

const char* keys[11] = {key_http_method, key_consumer_key, key_nonce, key_signature_method, key_timestamp, key_token, key_version, key_status, key_signature, value_signature_method, value_version};

const char* fingerprint = "D8 01 5B F4 6D FB 91 C6 E4 B1 B6 AB 9A 72 C1 68 93 3D C2 D9";

// for image upload
const char* upload_filename = "/0416.jpg";

const char upload_base_host[]    = "upload.twitter.com";
const char upload_base_url[]     = "https://upload.twitter.com/1.1/media/upload.json";
const char upload_base_uri[]     = "/1.1/media/upload.json";
const char upload_fingerprint[]  = "95 00 10 59 C8 27 FD 2C D0 76 12 F7 88 35 64 21 F5 60 D3 E9";
const char upload_oauth_key[]    = "oauth_body_hash";

const char* imagearray[5] = { upload_base_host, upload_base_url, upload_base_uri, upload_fingerprint, upload_oauth_key };

WiFiClientSecure client;
WiFiUDP udp;

unsigned int localPort = 12390;
const int timeZone     = 9;

const char* filename = "/tweet_done.txt";
bool x;

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

  bool result = SPIFFS.begin();
  Serial.println();
  Serial.println(" ");
  Serial.printf("[SPIFFS] opened: %d\n", result);

  File f = SPIFFS.open(filename, "r");
  if (!f) {
    Serial.println("[SPIFFS] do not tweet");
    int len = f.size();
    x = false;
  } else {
    Serial.println("[SPIFFS] do tweet");
    File f = SPIFFS.open(filename, "w");
    f.println("done");
    x = true;
  }
  f.close();

  File fi = SPIFFS.open(upload_filename, "r");
  if (!fi) {
    Serial.println("[SPIFFS] no file");
  } else {
    Serial.println("[SPIFFS] image to tweet");
    Serial.printf("[SPIFFS] file.name(): %s\n", fi.name());
    Serial.printf("[SPIFFS] file.size(): %d\n", fi.size());
  }
  fi.close();
}

time_t prevDisplay = 0;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      if (timeStatus() == timeSet) {
        digitalClockDisplay();

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
  const char* value_status  = "esp-01 + direct tweet / test 4";
  tweeting(value_status, upload_filename);
  //tweeting(value_status, "None");
}

void tweeting(const char* value_status, String image_file_name) {
  bool image = true;
  if (image_file_name == "None" ) {
    Serial.println("[TWEET] : TEXT");
    image = false;
  } else {
    Serial.println("[TWEET] : IMAGE");
  }

  uint32_t value_timestamp  = now();
  uint32_t value_nonce      = *(volatile uint32_t *)0x3FF20E44;

  int content_length;
  String hashStr = "";

  String status_all = make_status_all(value_status, hashStr);

  if (image) {
    String content_more = "--00Twurl19799557735000475lruwT19\r\n" ;
    content_more += "Content-Disposition: form-data; name=\"media\"; filename=\"" + String(value_timestamp) + ".jpg\"\r\n" ;
    content_more += "Content-Type: application/octet-stream\r\n\r\n";

    String content_last = "\r\n--00Twurl19799557735000475lruwT19--\r\n";

    File f = SPIFFS.open(image_file_name, "r");
    if (!f) {
      Serial.println("[SPIFFS] File doesn't exist yet");
    } else {
      char buff[128] = { 0 };

      content_length = len + content_more.length() + content_last.length() ;

      Serial.print("[SPIFFS] file size : ");
      Serial.println(len);
      Serial.print("[SPIFFS] content_length : ");
      Serial.println(content_length);


      uint8_t digestkey[32];
      SHA1_CTX context;
      SHA1_Init(&context);

      while (f.available()) {
        int c = f.readBytes(buff, ((len > sizeof(buff)) ? sizeof(buff) : len));
        SHA1_Update(&context, (uint8_t*) buff, c);
        if (len > 0) {
          len -= c;
        }
      }
      SHA1_Final(digestkey, &context);
      f.close();
       
      Serial.print("[HASH] sha1  :");
      for (uint16_t i = 0; i < 20; i++) {
        String hex = String(digestkey[i], HEX);
        if (hex.length() < 2) {
          hex = "0" + hex;
        }
        hashStr += hex;
      }

      Serial.println(hashStr);
      Serial.print("[HASH] should:");
      Serial.println("27f156fe3cf6d7c050cbb969163749dad55611bd");
      Serial.println();
      Serial.flush();
    }

    // needed ?
    hashStr = base64::encode((uint8_t*)hashStr.c_str(), 20);

    String para_string = make_para_string(status_all, value_nonce, value_timestamp, hashStr);
    String base_string = make_base_string(para_string, hashStr);
    String oauth_signature = make_signature(tweeter[1], tweeter[3], base_string);
    String OAuth_header = make_OAuth_header(oauth_signature, value_nonce, value_timestamp, hashStr);

    Serial.print("[Oauth] para_string : ");
    Serial.println(para_string);
    Serial.print("[Oauth] base_string : ");
    Serial.println(base_string);
    Serial.print("[Oauth] oauth_signature : ");
    Serial.println(oauth_signature);
    Serial.print("[Oauth] OAuth_header : ");
    Serial.println(OAuth_header);


    String content_header = "POST " + String(imagearray[2]) + " HTTP/1.1\r\n";
    content_header += "Accept-Encoding: identity;q=1,chunked;q=0.1,*;q=0\r\n";
    content_header += "User-Agent: esp8266_image_bot_by_chaeplin_V_0.1\r\n";
    content_header += "Content-Type: multipart/form-data, boundary=\"00Twurl19799557735000475lruwT19\"\r\n";
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

    do_http_image_post(tweeter[4], OAuth_header, image_file_name, content_header, content_more, content_last);

  } else {

  }
  /*
    String para_string = make_para_string(status_all, value_nonce, value_timestamp, hashStr);
    String base_string = make_base_string(para_string, hashStr);

    String oauth_signature = make_signature(tweeter[1], tweeter[3], base_string);
    String OAuth_header = make_OAuth_header(oauth_signature, value_nonce, value_timestamp);



    Serial.print("[Oauth] para_string : ");
    Serial.println(para_string);
    Serial.print("[Oauth] base_string : ");
    Serial.println(base_string);
    Serial.print("[Oauth] oauth_signature : ");
    Serial.println(oauth_signature);
    Serial.print("[Oauth] OAuth_header : ");
    Serial.println(OAuth_header);
  */


  // do_http_post(tweeter[4], OAuth_header, status_all, image_file_name);
}



String make_status_all(const char* value_status, String hashStr) {
  String status_all = keys[7];
  status_all += "=";
  status_all += URLEncode(value_status);
  return status_all;
}

String make_para_string(String status_all, uint32_t value_nonce, uint32_t value_timestamp, String hashStr) {
  String para_string;
  if (hashStr.length() > 0 ) {
    para_string += imagearray[4];
    para_string += "=" ;
    para_string += URLEncode(hashStr.c_str());
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
    /*
      String base_string = keys[0];
      base_string += "&";
      base_string += URLEncode(imagearray[2]);
      base_string += "&";
      base_string += URLEncode(para_string.c_str());
    */
    String base_string = URLEncode(para_string.c_str());
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

uint32_t do_http_image_post(const char* basehost, String OAuth_header, String image_file_name, String content_header, String content_more, String content_last) {
  Serial.print("[HTTP] start millis     : ");
  Serial.println(millis());

  File f = SPIFFS.open(image_file_name, "r");
  if (!f) {
    Serial.println("[SPIFFS] File doesn't exist yet");
    return false;
  }

  char buff[1024] = { 0 };
  int len = f.size();

  Serial.print("connecting to ");
  Serial.println(imagearray[0]);
  if (!client.connect(imagearray[0], httpsPort)) {
    Serial.println("connection failed");
    return false;
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

  client.print(content_header);
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
  
  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  /*
    if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
    } else {
    Serial.println("esp8266/Arduino CI has failed");
    }
  */
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");

  return 12345;

}

bool do_http_text_post(const char* basehost, String OAuth_header, String status_all) {
  HTTPClient http;
  http.begin(basehost, httpsPort, base_uri, fingerprint);
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
    return true;
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
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
  Serial.println(year());
}

void printDigits(int digits) {
  Serial.print(": ");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// end
