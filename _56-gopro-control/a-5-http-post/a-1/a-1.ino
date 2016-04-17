#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
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

const char* consumer_key    = ConsumerKey;
const char* consumer_secret = ConsumerSecret;
const char* access_token    = AccessToken;
const char* access_secret   = AccessSecret;

const char* base_host        = "api.twitter.com";
const char* base_url         = "https://api.twitter.com/1.1/statuses/update.json";
const int httpsPort          = 443;

//const char* fingerprint = "CF 05 98 89 CA FF 8E D8 5E 5C E0 C2 E4 F7 E6 C3 C7 50 DD 5C";

//WiFiClient client;
WiFiClientSecure client;
WiFiUDP udp;

unsigned int localPort = 12390;
const int timeZone     = 9;

bool x = false;

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

  WiFiClient::setLocalPortStart(analogRead(A0));
  wifi_connect();

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    setSyncProvider(getNtpTime);
  }
}

time_t prevDisplay = 0;

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      if (timeStatus() == timeSet) {
        digitalClockDisplay();
      }
    }
    if (!x) {
      tweeting();
      x = true;
    }
  } else {
    wifi_connect();
    setSyncProvider(getNtpTime);
  }
}

void tweeting() {


  const char* _status    = "Hello Ladies + Gentlemen, a signed OAuth request!";
  //const char* _status    = "status=Hello Ladies + Gentlemen, a signed OAuth request!";
  String oauth_timestamp      = String(now());
  //String oauth_timestamp    = "1460830649";

  //String oauth_nonce        =  base64::encode(oauth_timestamp).c_str();
  //String oauth_nonce        = oauth_url_escape(base64::encode((uint8_t *) * (volatile uint32_t *)0x3FF20E44, 20).c_str());
  //String oauth_nonce        = "cf582422eae65a642aaac2d622ace992";
  uint32_t oauth_nonce        = *(volatile uint32_t *)0x3FF20E44;
  String oauth_consumer_key   = oauth_url_escape(consumer_key);
  String oauth_token          = oauth_url_escape(access_token);
  String oauth_status         = oauth_url_escape(_status);
  String _status_all          = "status=";
  _status_all += _status;

  String para_string = "oauth_consumer_key=";
  para_string += oauth_consumer_key;
  para_string += "&oauth_nonce=";
  para_string += oauth_nonce;
  para_string += "&oauth_signature_method=HMAC-SHA1";
  para_string += "&oauth_timestamp=";
  para_string += oauth_timestamp;
  para_string += "&oauth_token=";
  para_string += oauth_token;
  para_string += "&oauth_version=1.0&status=";
  para_string += oauth_status;

  Serial.println(para_string);

  String base_string = "POST&";
  base_string += oauth_url_escape(base_url);
  base_string += "&";
  base_string += oauth_url_escape(para_string.c_str());

  Serial.println(base_string);

  String signing_key = oauth_url_escape(consumer_secret);
  signing_key += "&";
  signing_key += oauth_url_escape(access_secret);

  Serial.println(signing_key);

  uint8_t digestkey[32];
  SHA1_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) signing_key.c_str(), (int)signing_key.length());
  SHA1_Final(digestkey, &context);

  uint8_t digest[32];
  ssl_hmac_sha1((uint8_t*) base_string.c_str(), (int)base_string.length(), digestkey, SHA1_SIZE, digest);

  String oauth_signature = base64::encode(digest, SHA1_SIZE);
  Serial.println(oauth_signature);
  Serial.println(oauth_url_escape(oauth_signature.c_str()));

  /*
    Serial.print("connecting to ");
    Serial.println(base_host);
    if (!client.connect(base_host, httpsPort)) {
    Serial.println("connection failed");
    return;
    }
  */

  /*
    if (client.verify(fingerprint, base_host)) {
    Serial.println("certificate matches");
    } else {
    Serial.println("certificate doesn't match");
    }
  */


  return;

  
  String url = "/1.1/statuses/update.json";
// "--data 'status=Hello+Ladies+++Gentlemen%2C+a+signed+OAuth+request%21' " +
// "--data '" +  URLEncode(_status_all.c_str()) + "' " +
  Serial.println(String("curl ") + "--request 'POST' 'https://api.twitter.com/1.1/statuses/update.json' " +
                  "--data 'status=Hello+Ladies%2C+Gentlemen%2C+a+signed+OAuth+request+test+2' " +
                 "--header 'Authorization: OAuth " +
                 "oauth_consumer_key=\"" + consumer_key +
                 "\", oauth_nonce=\"" + oauth_nonce +
                 "\", oauth_signature=\"" + oauth_url_escape(oauth_signature.c_str()) +
                 "\", oauth_signature_method=\"HMAC-SHA1\"," +
                 " oauth_timestamp=\"" + oauth_timestamp +
                 "\", oauth_token=\"" + access_token +
                 "\", oauth_version=\"1.0\"' --verbose");




  
  
  //Serial.print("requesting URL: ");
  //Serial.println(url);

  Serial.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + base_host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Authorization: OAuth oauth_consumer_key=\"" +
               consumer_key + "\", oauth_nonce=\"" +
               oauth_nonce + "\", oauth_signature=\"" +
               URLEncode(oauth_signature.c_str()) + "\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\"" +
               oauth_timestamp + "\", oauth_token=\"" +
               access_token + "\", oauth_version=\"1.0\"\r\n" +
               "Content-Length: " + sizeof(oauth_url_escape(_status_all.c_str())) + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n\r\n") ; //  +
  //"Connection: close\r\n\r\n");

  client.print(String("POST ") + url + " HTTP/1.1\r\n" +
               "Host: " + base_host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Authorization: OAuth oauth_consumer_key=\"" +
               consumer_key + "\", oauth_nonce=\"" +
               oauth_nonce + "\", oauth_signature=\"" +
               URLEncode(oauth_signature.c_str()) + "\", oauth_signature_method=\"HMAC-SHA1\", oauth_timestamp=\"" +
               oauth_timestamp + "\", oauth_token=\"" +
               access_token + "\", oauth_version=\"1.0\"\r\n" +
               "Content-Length: " + sizeof(oauth_url_escape(_status_all.c_str())) + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n\r\n") ; //  +
  //"Connection: close\r\n\r\n");

  Serial.println(oauth_url_escape(_status_all.c_str()));
  client.print(oauth_url_escape(_status_all.c_str()));
  
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
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

// from liboauth-1.0.3
char *oauth_url_escape(const char *string) {
  size_t alloc, newlen;
  char *ns = NULL, *testing_ptr = NULL;
  unsigned char in; 
  size_t strindex=0;
  size_t length;


  alloc = strlen(string)+1;
  newlen = alloc;

  ns = (char*) malloc(alloc);

  length = alloc-1;
  while(length--) {
    in = *string;

    switch(in){
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'a': case 'b': case 'c': case 'd': case 'e':
    case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o':
    case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E':
    case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O':
    case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_': case '~': case '.': case '-':
      ns[strindex++]=in;
      break;
    default:
      newlen += 2; /* this'll become a %XX */
      if(newlen > alloc) {
        alloc *= 2;
        testing_ptr = (char*) realloc(ns, alloc);
        ns = testing_ptr;
      }
      snprintf(&ns[strindex], 4, "%%%02X", in);
      strindex+=3;
      break;
    }
    string++;
  }
  ns[strindex]=0;
  return ns;
}


// END
