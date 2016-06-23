// 80M / esp-01 1M / 64K SPIFFS /
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#include "/usr/local/src/ap_setting.h"

extern "C"
{
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define IPSET_STATIC  { 192, 168, 10, 16 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET  { 255, 255, 255, 0 }
#define IPSET_DNS     { 192, 168, 10, 10 }

#define I2C_SLAVE_ADDR  8
#define I2C_MATRIX_ADDR 0x70

#define HT16K33_SS            B00100000 // System setup register
#define HT16K33_SS_STANDBY    B00000000 // System setup - oscillator in standby mode
#define HT16K33_SS_NORMAL     B00000001 // System setup - oscillator in normal mode

uint8_t HT16K33_i2c_write(uint8_t val) {
  Wire.beginTransmission(I2C_MATRIX_ADDR);
  Wire.write(val);
  return Wire.endTransmission();
} // _i2c_write

// Put the chip to sleep
//
uint8_t HT16K33_sleep() {
  return HT16K33_i2c_write(HT16K33_SS | HT16K33_SS_STANDBY); // Stop oscillator
} // sleep

/****************************************************************/
// Wake up the chip (after it been a sleep )
//
uint8_t HT16K33_normal() {
  return HT16K33_i2c_write(HT16K33_SS | HT16K33_SS_NORMAL); // Start oscillator
} // normal

typedef struct
{
  uint32_t hash;
  uint16_t button;
  uint16_t esp8266;
} data;

volatile data device_esp;
volatile data device_pro;

volatile bool haveData = false;

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress mqtt_server = MQTT_SERVER;
//byte mqtt_server[] = MQTT_SERVER;

//
IPAddress ip_static = IPSET_STATIC;
IPAddress ip_gateway = IPSET_GATEWAY;
IPAddress ip_subnet = IPSET_SUBNET;
IPAddress ip_dns = IPSET_DNS;
// ****************

char* topic = "esp8266/cmd/light";
char* subtopic = "esp8266/cmd/light/rlst";
char* buttontopic = "button";

char* actopic = "esp8266/cmd/ac";

String clientName;
String payload;

volatile int subMsgReceived = LOW;
volatile int topicMsgSent   = LOW;
volatile int relaystatus    = LOW;
volatile int relayReady     = LOW;

int vdd;

unsigned long startMills = 0;
unsigned long wifiMills = 0;
unsigned long subMills = 0;

long lastReconnectAttempt = 0;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
Adafruit_8x8matrix matrix = Adafruit_8x8matrix();

/* for hash */
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length)
{
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data)
{
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

/* for i2c */
template <typename T> unsigned int I2C_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = Wire.read();
  return i;
}

template <typename T> unsigned int I2C_writeAnything (const T& value)
{
  Wire.write((byte *) &value, sizeof (value));
  return sizeof (value);
}


static const uint8_t PROGMEM
smile_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10100101,
  B10011001,
  B01000010,
  B00111100
},
neutral_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10111101,
  B10000001,
  B01000010,
  B00111100
},
frown_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10011001,
  B10100101,
  B01000010,
  B00111100
},
light[] =
{ B00111100,
  B01000010,
  B10000101,
  B10001001,
  B10010001,
  B01100110,
  B00011000,
  B00011000
},
ac[] =
{ B01111111,
  B11000000,
  B10111110,
  B10100000,
  B10100111,
  B10110100,
  B11000000,
  B01111111
},
act[] =
{ B01111111,
  B11000000,
  B10111110,
  B10100000,
  B10100111,
  B10110010,
  B11000010,
  B01111111
},
large_heart[] =
{ B00000000,
  B01100110,
  B11111111,
  B11111111,
  B01111110,
  B00111100,
  B00011000,
  B00000000
},
small_heart[] =
{ B00000000,
  B00000000,
  B00100100,
  B01011010,
  B01000010,
  B00100100,
  B00011000,
  B00000000
},
fail_heart[] =
{ B00000000,
  B01100110,
  B10011101,
  B10001001,
  B01010010,
  B00100100,
  B00011000,
  B00000000
};

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++)
  {
    receivedpayload += (char)inpayload[i];
  }

  if ( receivedpayload == "{\"LIGHT\":1,\"READY\":1}")
  {
    relaystatus = HIGH ;
    relayReady = HIGH ;
  }
  else if ( receivedpayload == "{\"LIGHT\":0,\"READY\":1}")
  {
    relaystatus = LOW ;
    relayReady = HIGH ;
  }
  else if ( receivedpayload == "{\"LIGHT\":1,\"READY\":0}")
  {
    relayReady = LOW ;
  } else if ( receivedpayload == "{\"LIGHT\":0,\"READY\":0}")
  {
    relayReady = LOW ;
  }

  subMsgReceived = HIGH;
}

boolean reconnect()
{
  if (client.connect((char*) clientName.c_str()))
  {
    client.subscribe(subtopic);
  }
  else
  {
    ;
  }
  return client.connected();
}

void wifi_connect()
{
  WiFiClient::setLocalPortStart(micros() + vdd);
  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  WiFi.config(ip_static, ip_gateway, ip_subnet);
  WiFi.begin(ssid, password);
  //WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));
  WiFi.hostname("esp-button");

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Attempt++;
    if (Attempt == 150)
    {
      goingToSleepWithFail();
    }
  }

  wifiMills = millis() - startMills;

  Serial.print("[WIFI] connected millis : ");
  Serial.print(millis());
  Serial.print(" - ");
  Serial.println(WiFi.localIP());
}

void setup() {
  startMills = millis();

  Serial.begin(115200);
  Serial.println();
  Serial.flush();
  Serial.println("Starting.....");

  wifi_connect();

  vdd = ESP.getVcc() ;

  device_esp.button = device_esp.esp8266 = 0;
  device_esp.hash = calc_hash(device_esp);

  Wire.begin(0, 2);
  twi_setClock(200000);

  HT16K33_normal();

  matrix.begin(I2C_MATRIX_ADDR);
  matrix.setBrightness(10);
  matrix.setRotation(3);

  matrix.clear();
  matrix.drawRect(0, 0, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(200);

  matrix.fillRect(2, 2, 4, 4, LED_ON);
  matrix.writeDisplay();
  delay(200);

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  reconnect();

  int Attempt = 0;
  while (1)
  {
    Serial.print("request msg device_pro.button : ");
    readI2cMsg();
    Serial.println(device_pro.button);

    Serial.print("haveData : ");
    Serial.print(haveData);
    Serial.print(" hash : ");
    Serial.print(device_pro.hash);
    Serial.print(" : ");
    Serial.println(calc_hash(device_pro));
    
    Attempt++;
    if (haveData && (device_pro.button != 0))
    {
      break;
    }
    
    if (Attempt == 8)
    {
      goingToSleepWithFail();
    }
    delay(200);
  }

  matrix.clear();
  switch (device_pro.button)
  {
    case 1:
      matrix.drawBitmap(0, 0, light, 8, 8, LED_ON);
      break;

    case 2:
      matrix.drawBitmap(0, 0, ac, 8, 8, LED_ON);
      break;

    case 3:
      matrix.drawBitmap(0, 0, act, 8, 8, LED_ON);
      break;

    default:
      matrix.drawBitmap(0, 0, smile_bmp, 8, 8, LED_ON);
      break;
  }
  matrix.writeDisplay();
  delay(1000);
}

void loop() {    
  if (WiFi.status() == 3)
  {
    if (!client.connected())
    {
      long now = millis();
      if (now - lastReconnectAttempt > 200)
      {
        lastReconnectAttempt = now;
        if (reconnect())
        {
          lastReconnectAttempt = 0;
        }
      }
    }
    else
    {
      if ( topicMsgSent == HIGH )
      {
        goingToSleep();
      }
      else
      {
        switch (device_pro.button)
        {
          case 1:
            if (( subMsgReceived == HIGH ) && ( relayReady == HIGH ))
            {
              subMills = (millis() - startMills) - wifiMills ;
              sendlightcmd();
            }

            if (( subMsgReceived == HIGH ) && ( relayReady == LOW ))
            {
              subMills = (millis() - startMills) - wifiMills ;
              goingToSleepWithFail();
            }
            break;

          case 2:
            sendaccommand();
            break;

          case 3:
            sendaccommand();
            break;

          case 4:
            goingToSleepWithFail();
            break;

          default:
            goingToSleepWithFail();
            break;
        }
      }

      if ((millis() - startMills) > 15000)
      {
        goingToSleepWithFail();
      }

      client.loop();
    }
  }

  if ((millis() - startMills) > 15000)
  {
    goingToSleepWithFail();
  }
}

void sendaccommand()
{
  int i = 5;
  String acpayload = "{\"AC\":";
  acpayload += i;
  acpayload += "}";

  sendmqttMsg(actopic, acpayload);
  sendbuttonstatus();
}

void sendlightcmd()
{
  String lightpayload = "{\"LIGHT\":";
  lightpayload += !relaystatus;
  lightpayload += "}";

  sendmqttMsg(topic, lightpayload);
  sendbuttonstatus();
}

void sendbuttonstatus()
{
  String buttonpayload = "{\"VDD\":";
  buttonpayload += vdd;
  buttonpayload += ",\"buttonmillis\":";
  buttonpayload += (millis() - startMills) ;
  buttonpayload += ",\"wifiMills\":";
  buttonpayload += wifiMills ;
  buttonpayload += ",\"subMills\":";
  buttonpayload += subMills ;
  buttonpayload += "}";

  if (sendmqttMsg(buttontopic, buttonpayload))
  {
    topicMsgSent = HIGH;
  }
}

boolean sendmqttMsg(char* topictosend, String payload)
{
  if (client.connected())
  {
    unsigned int msg_length = payload.length();
    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1))
    {
      free(p);
      return 1;
    }
    else
    {
      free(p);
      return 0;
    }
  }
}

void readI2cMsg()
{
  if (Wire.requestFrom(I2C_SLAVE_ADDR, sizeof(device_pro)))
  {
    I2C_readAnything(device_pro);
  }
  if (device_pro.hash == calc_hash(device_pro))
  {
    haveData = true;
  }
}

void sendI2cMsg()
{
  Serial.println("Send msg");

  device_esp.esp8266 = 3;
  device_esp.hash = calc_hash(device_esp);

  Wire.beginTransmission(I2C_SLAVE_ADDR);
  I2C_writeAnything(device_esp);
  Wire.endTransmission();
}

void goingToSleep()
{
  if (client.connected())
  {
    client.disconnect();
    yield();
  }
  //delay(100);

  matrix.clear();
  matrix.drawBitmap(0, 0, large_heart, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(400);

  sendI2cMsg();
  delay(100);

  HT16K33_sleep();
  delay(100);

  ESP.deepSleep(0);
  delay(50);
}


void goingToSleepWithFail()
{
  if (client.connected())
  {
    client.disconnect();
    yield();
  }
  //delay(100);

  matrix.clear();
  matrix.drawBitmap(0, 0, fail_heart, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(400);

  sendI2cMsg();
  delay(100);

  HT16K33_sleep();
  delay(100);

  ESP.deepSleep(0);
  delay(50);
}


String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

//-----
