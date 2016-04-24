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

  //#include "user_interface.h"
}
const char* upload_filename = "/0416.jpg";

void setup() {

  Serial.begin(115200);
  Serial.println();

  bool result = SPIFFS.begin();
  Serial.println();
  Serial.println(" ");
  Serial.printf("[SPIFFS] opened: %d\n", result);


  File f = SPIFFS.open(upload_filename, "r");
  if (!f) {
    Serial.println("[SPIFFS] File doesn't exist yet");
  } else {
    char buff[1024] = { 0 };
    int len = f.size();

    String content_more = "--00Twurl15632260651985405lruwT99\r\n" ;
    content_more += "Content-Disposition: form-data; name=\"media\"; filename=\"04163.jpg\"\r\n";
    content_more += "Content-Type: application/octet-stream\r\n\r\n";

    String content_last = "\r\n--00Twurl15632260651985405lruwT99--\r\n";
    int content_length = len + content_more.length() + content_last.length();

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

    Serial.print("raw digestkey :");
    Serial.println(base64::encode(digestkey, 20));
    
    Serial.println();
    Serial.print("sha1  :");
    String hashStr = "";
    for(uint16_t i = 0; i < 20; i++) {
        String hex = String(digestkey[i], HEX);
        if(hex.length() < 2) {
            hex = "0" + hex;
        }
        hashStr += hex;
    }

    Serial.println(hashStr);
    Serial.print("should:");
    Serial.println("27f156fe3cf6d7c050cbb969163749dad55611bd");
    Serial.println();
    Serial.flush();
  }
}


void loop() {

}
