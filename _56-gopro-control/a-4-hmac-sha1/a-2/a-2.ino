

#include <base64.h>
#include "/usr/local/src/ap_setting.h"

#define SHA1_SIZE 20

extern "C" {
  // https://github.com/mharizanov/tweeting_silicon/blob/master/include/ssl_crypto.h
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

  void hmac_sha1(const uint8_t *msg, int length, const uint8_t *key, int key_len, uint8_t *digest);
}

void compute();
String URLEncode(const char* msg);

String base_string_a = "POST&https%3A%2F%2Fapi.twitter.com%2F1%2Fstatuses%2Fupdate.json&include_entities%3Dtrue%26oauth_consumer_key%3Dxvz1evFS4wEEPTGEFPHBog%26oauth_nonce%3DkYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1318622958%26oauth_token%3D370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb%26oauth_version%3D1.0%26status%3DHello%2520Ladies%2520%252B%2520Gentlemen%252C%2520a%2520signed%2520OAuth%2520request%2521";
String signing_key_a = "kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw&LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE";
String para_string_a = "include_entities=true&oauth_consumer_key=xvz1evFS4wEEPTGEFPHBog&oauth_nonce=kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg&oauth_signature_method=HMAC-SHA1&oauth_timestamp=1318622958&oauth_token=370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb&oauth_version=1.0&status=Hello%20Ladies%20%2B%20Gentlemen%2C%20a%20signed%20OAuth%20request%21";

// --> B679C0AF18F4E9C587AB8E200ACD4E48A93F8CB6
// --> tnnArxj06cWHq44gCs1OSKk/jLY=


const char* consumer_key    = "xvz1evFS4wEEPTGEFPHBog";
const char* consumer_secret = "kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw";
const char* access_token    = "370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb";
const char* access_secret   = "LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE";

const char* oauth_nonce     = "kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg";
const char* oauth_timestamp = "1318622958";
const char* oauth_status    = "Hello Ladies + Gentlemen, a signed OAuth request!";
const char* base_url        = "https://api.twitter.com/1/statuses/update.json";

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println();

  String para_string = "include_entities=true&oauth_consumer_key=";
  para_string += URLEncode(consumer_key);
  para_string += "&oauth_nonce=";
  para_string += URLEncode(oauth_nonce);
  para_string += "&oauth_signature_method=HMAC-SHA1&oauth_timestamp=";
  para_string += URLEncode(oauth_timestamp);
  para_string += "&oauth_token=";
  para_string += URLEncode(access_token);
  para_string += "&oauth_version=1.0&status=";
  para_string += URLEncode(oauth_status);


  if ( para_string != para_string_a ) {
    Serial.println("different result");

  } else {
    Serial.println("same result");
  }
  Serial.println(para_string);
  Serial.println(para_string_a);

  String base_string = "POST&";
  base_string += URLEncode(base_url);
  base_string += "&";
  base_string += URLEncode(para_string.c_str());

  if ( base_string != base_string_a) {
    Serial.println("different result");

  } else {
    Serial.println("same result");
  }
  Serial.println(base_string);
  Serial.println(base_string_a);


  String signing_key = URLEncode(consumer_secret);
  signing_key += "&";
  signing_key += URLEncode(access_secret);

  if ( signing_key != signing_key_a ) {
    Serial.println("different result");

  } else {
    Serial.println("same result");
  }
  Serial.println(signing_key);
  Serial.println(signing_key_a);


  //https://github.com/mharizanov/tweeting_silicon/blob/master/user/user_main.c
  uint8_t digestkey[32];
  SHA1_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) signing_key.c_str(), (int)signing_key.length());
  SHA1_Final(digestkey, &context);

  uint8_t digest[32];
  hmac_sha1((uint8_t*) base_string.c_str(), (int)base_string.length(), digestkey, SHA1_SIZE, digest);

  Serial.print("OAuth signature ssl_hmac_sha1: ");
  for (int i = 0; i < SHA1_SIZE; i++) {
    Serial.printf("%02X", digest[i]);
  }
  Serial.println();

  Serial.println("OAuth signature should be :  tnnArxj06cWHq44gCs1OSKk/jLY=");
  Serial.print("Result ------------------>:  ");
  Serial.println(base64::encode(digest, SHA1_SIZE));
  Serial.println();
}

void loop() {

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
      encodedMsg += hex[*msg & 15];
    }
    msg++;
  }
  return encodedMsg;
}

