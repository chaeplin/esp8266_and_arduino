// https://dev.twitter.com/oauth/overview/creating-signatures
/*
   oauth_consumer_key : xvz1evFS4wEEPTGEFPHBog
   oauth_token : 370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb
   Consumer secret : kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw
   OAuth token secret : LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE

   https://api.twitter.com/1/statuses/update.json.

     POST /1/statuses/update.json?include_entities=true HTTP/1.1
     Connection: close
     User-Agent: OAuth gem v0.4.4
     Content-Type: application/x-www-form-urlencoded
     Content-Length: 76
     Host: api.twitter.com

    OAuth signature  tnnArxj06cWHq44gCs1OSKk/jLY=
*/

#include <base64.h>
#define SHA1_SIZE   20

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
}

// https://github.com/igrr/axtls-8266/blob/master/crypto/hmac.c
void ssl_hmac_sha1(const uint8_t *msg, int length, const uint8_t *key, int key_len, uint8_t *digest)
{
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

/*
  //const char* http_method = "POST";
  //const char* base_url = "https://api.twitter.com/1/statuses/update.json";

  const char* para_status_vlaue = "Hello Ladies + Gentlemen, a signed OAuth request!";
  const char* para_query_key = "include_entities";
  const char* para_query_value = "true";
  const char* para_oauth_consumer_key = "xvz1evFS4wEEPTGEFPHBog";
  const char* para_oauth_nonce = "kYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg";
  const char* para_oauth_signature_method = "HMAC-SHA1";
  const char* para_oauth_timestamp = "1318622958";
  const char* para_oauth_oauth_token = "370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb";
  const char* para_oauth_version = "1.0";

  const char* Consumer_secret = "kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw";
  const char* Token_secret = "LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE";
*/

String base_string = "POST&https%3A%2F%2Fapi.twitter.com%2F1%2Fstatuses%2Fupdate.json&include_entities%3Dtrue%26oauth_consumer_key%3Dxvz1evFS4wEEPTGEFPHBog%26oauth_nonce%3DkYjzVBB8Y0ZFabxSWbWovY3uYSQ2pTgmZeNu2VS4cg%26oauth_signature_method%3DHMAC-SHA1%26oauth_timestamp%3D1318622958%26oauth_token%3D370773112-GmHxMAgYyLbNEtIKZeRNFsMKPR9EyMZeS9weJAEb%26oauth_version%3D1.0%26status%3DHello%2520Ladies%2520%252B%2520Gentlemen%252C%2520a%2520signed%2520OAuth%2520request%2521";
String signing_key = "kAcSOqF21Fu85e7zjz7ZN2U4ZRhfV3WpwPAoE3Z7kBw&LswwdoUaIvS8ltyTt5jkRh4J50vUPVVHtR2YPi5kE";

// --> B679C0AF18F4E9C587AB8E200ACD4E48A93F8CB6
// --> tnnArxj06cWHq44gCs1OSKk/jLY=


// from http://hardwarefun.com/tutorials/url-encoding-in-arduino
String URLEncode(const char* msg)
{
  const char *hex = "0123456789abcdef";
  String encodedMsg = "";

  while (*msg != '\0') {
    if ( ('a' <= *msg && *msg <= 'z')
         || ('A' <= *msg && *msg <= 'Z')
         || ('0' <= *msg && *msg <= '9') ) {
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

// https://github.com/mharizanov/tweeting_silicon/blob/master/user/user_main.c
void compute() {
  uint8_t digestkey[32];
  SHA1_CTX context;
  SHA1_Init(&context);
  SHA1_Update(&context, (uint8_t*) signing_key.c_str(), (int)signing_key.length());
  SHA1_Final(digestkey, &context);

  uint8_t digest[32];
  ssl_hmac_sha1((uint8_t*) base_string.c_str(), (int)base_string.length(), digestkey, SHA1_SIZE, digest);

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



void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }
  Serial.println();
  compute();
}

void loop() {


}

