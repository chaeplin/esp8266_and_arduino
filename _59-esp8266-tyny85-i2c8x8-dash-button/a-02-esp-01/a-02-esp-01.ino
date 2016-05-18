// esp-01 1M / 64K SPIFFS
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define IPSET_STATIC { 192, 168, 10, 16 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

#define I2C_SLAVE_ADDR  0x26            // i2c slave address (38, 0x26)
#define I2C_MATRIX_ADDR 0x70

#define HT16K33_SS            B00100000 // System setup register
#define HT16K33_SS_STANDBY    B00000000 // System setup - oscillator in standby mode
#define HT16K33_SS_NORMAL     B00000001 // System setup - oscillator in normal mode

Adafruit_8x8matrix matrix = Adafruit_8x8matrix();

// ------------------
void callback(char* intopic, byte* inpayload, unsigned int length);
void goingToSleepWithFail();
String macToStr(const uint8_t* mac);
void goingToSleep();
void sendlightcmd();
boolean sendmqttMsg(char* topictosend, String payload);

// #define MQTT_KEEPALIVE 3 in PubSubClient.h

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
int32_t channel = WIFI_CHANNEL;
//byte bssid[] = WIFI_BSSID;
byte mqtt_server[] = MQTT_SERVER;
//
byte ip_static[] = IPSET_STATIC;
byte ip_gateway[] = IPSET_GATEWAY;
byte ip_subnet[] = IPSET_SUBNET;
byte ip_dns[] = IPSET_DNS;
// ****************

char* topic = "esp8266/cmd/light";
char* subtopic = "esp8266/cmd/light/rlst";
char* buttontopic = "button";

String clientName;
String payload;

volatile int subMsgReceived = LOW;
volatile int topicMsgSent   = LOW;
volatile int relaystatus    = LOW;
volatile int relayReady     = LOW;

int vdd;

//
unsigned long startMills = 0;
unsigned long wifiMills = 0;
unsigned long subMills = 0;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

long lastReconnectAttempt = 0;

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
};

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  if ( receivedpayload == "{\"LIGHT\":1,\"READY\":1}") {
    relaystatus = HIGH ;
    relayReady = HIGH ;
  } else if ( receivedpayload == "{\"LIGHT\":0,\"READY\":1}") {
    relaystatus = LOW ;
    relayReady = HIGH ;
  } else if ( receivedpayload == "{\"LIGHT\":1,\"READY\":0}") {
    relayReady = LOW ;
  } else if ( receivedpayload == "{\"LIGHT\":0,\"READY\":0}") {
    relayReady = LOW ;
  }

  subMsgReceived = HIGH;
}

boolean reconnect()
{
  if (client.connect((char*) clientName.c_str())) {
    client.subscribe(subtopic);
  } else {

  }
  return client.connected();
}

void wifi_connect()
{
  WiFiClient::setLocalPortStart(micros() + vdd);
  wifi_set_phy_mode(PHY_MODE_11N);
  //system_phy_set_rfoption(1);
  //wifi_set_channel(channel);

  if (WiFi.status() != WL_CONNECTED) {
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      //
      delay(100);
      //
      Attempt++;
      if (Attempt == 150)
      {
        goingToSleepWithFail();
      }
    }

    wifiMills = millis() - startMills;

    //wifi_set_sleep_type(LIGHT_SLEEP_T);
    //wifi_set_sleep_type(MODEM_SLEEP_T);

  }
}

uint8_t HT16K33_i2c_write(uint8_t val){
  Wire.beginTransmission(I2C_MATRIX_ADDR);
  Wire.write(val);
  return Wire.endTransmission();
} // _i2c_write


// Put the chip to sleep
//
uint8_t HT16K33_sleep(){
  return HT16K33_i2c_write(HT16K33_SS|HT16K33_SS_STANDBY); // Stop oscillator
} // sleep

/****************************************************************/
// Wake up the chip (after it been a sleep )
//
uint8_t HT16K33_normal(){
  return HT16K33_i2c_write(HT16K33_SS|HT16K33_SS_NORMAL); // Start oscillator
} // normal



void setup()
{
  Serial.swap();
  //
  startMills = millis();

  Wire.begin(2, 0);
  matrix.begin(I2C_MATRIX_ADDR);
  //HT16K33_normal();

  vdd = ESP.getVcc() ;

  matrix.setRotation(3);

  matrix.clear();      // clear display
  matrix.drawRect(0,0, 8,8, LED_ON);
  matrix.fillRect(2,2, 4,4, LED_ON);
  matrix.writeDisplay();  // write the changes we just made to the display
  
  wifi_connect();

  matrix.clear();
  matrix.drawBitmap(0, 0, smile_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;
  reconnect();
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 200) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    }
    /*
      else {
      client.loop();
      }
    */
  } else {
    wifi_connect();
  }

  if (( subMsgReceived == HIGH ) && ( relayReady == HIGH )) {
    subMills = (millis() - startMills) - wifiMills ;
    sendlightcmd();
  }

  if (( subMsgReceived == HIGH ) && ( relayReady == LOW )) {
    subMills = (millis() - startMills) - wifiMills ;
    goingToSleepWithFail();
  }

  if ( topicMsgSent == HIGH ) {
    goingToSleep();
  }

  if ((millis() - startMills) > 15000) {
    goingToSleepWithFail();
  }
  client.loop();


}

void sendlightcmd()
{
  String lightpayload = "{\"LIGHT\":";
  lightpayload += ! relaystatus;
  lightpayload += "}";

  if (sendmqttMsg(topic, lightpayload)) {

    String buttonpayload = "{\"VDD\":";
    buttonpayload += vdd;
    buttonpayload += ",\"buttonmillis\":";
    buttonpayload += (millis() - startMills) ;
    buttonpayload += ",\"wifiMills\":";
    buttonpayload += wifiMills ;
    buttonpayload += ",\"subMills\":";
    buttonpayload += subMills ;
    buttonpayload += "}";

    delay(20);
    if (sendmqttMsg(buttontopic, buttonpayload)) {
      topicMsgSent = HIGH;
    }
  }
}

boolean sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    unsigned int msg_length = payload.length();
    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      free(p);
      return 1;
    } else {
      free(p);
      return 0;
    }
  }
}

void sendI2cMsg() {
  byte msg = 1;
  Wire.beginTransmission(I2C_SLAVE_ADDR);
  Wire.write(msg);
  Wire.endTransmission();
}

void goingToSleep()
{
  client.disconnect();
  yield();

  matrix.clear();
  matrix.drawBitmap(0, 0, neutral_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(200);
  sendI2cMsg();
  delay(100);
  //HT16K33_sleep();
  //delay(100);
  
  ESP.deepSleep(0);
  delay(50);
}


void goingToSleepWithFail()
{
  sendI2cMsg();
  matrix.clear();
  matrix.drawBitmap(0, 0, frown_bmp, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(200);
  sendI2cMsg();
  delay(100);
  //HT16K33_sleep();
  //delay(100);  
  
  ESP.deepSleep(0);
  delay(50);
}


String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

//-----
