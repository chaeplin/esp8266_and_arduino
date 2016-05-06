// esp-01 1M / 64K SPIFFS
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define blueLED     1
#define greenLED    0
#define redLED      3

#define TINY85_SIG_PIN 2

#define IPSET_STATIC { 192, 168, 10, 16 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

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

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  digitalWrite(greenLED, HIGH);

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
  digitalWrite(greenLED, HIGH);
  if (client.connect((char*) clientName.c_str())) {
    client.subscribe(subtopic);
    digitalWrite(greenLED, LOW);
  } else {
    digitalWrite(redLED, HIGH);
    digitalWrite(redLED, LOW);
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
      digitalWrite(blueLED, HIGH);
      delay(50);
      digitalWrite(blueLED, LOW);
      delay(50);
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

void setup()
{
  Serial.swap();
  //
  startMills = millis();

  // pinmode setup
  pinMode(blueLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  pinMode(TINY85_SIG_PIN, OUTPUT);

  digitalWrite(redLED, HIGH);
  digitalWrite(TINY85_SIG_PIN, LOW);

  vdd = ESP.getVcc() ;

  digitalWrite(redLED, LOW);
  wifi_connect();

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
  digitalWrite(greenLED, HIGH);
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

  digitalWrite(greenLED, LOW);
  digitalWrite(blueLED, HIGH);

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
      digitalWrite(blueLED, LOW);
    }
  }
  digitalWrite(blueLED, HIGH);
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

void goingToSleep()
{
  client.disconnect();
  yield();
  //WiFi.disconnect();
  digitalWrite(greenLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(redLED, LOW);

  digitalWrite(TINY85_SIG_PIN, HIGH);
  ESP.deepSleep(0);
  delay(50);
}


void goingToSleepWithFail()
{
  digitalWrite(greenLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(redLED, LOW);

  digitalWrite(redLED, HIGH);
  delay(30);
  digitalWrite(redLED, LOW);
  delay(30);
  digitalWrite(redLED, HIGH);
  delay(30);
  digitalWrite(redLED, LOW);

  digitalWrite(TINY85_SIG_PIN, HIGH);
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
