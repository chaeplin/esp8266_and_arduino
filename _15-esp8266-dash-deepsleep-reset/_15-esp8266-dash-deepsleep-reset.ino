#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

extern "C" {
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define DEBUG_PRINT 0

#define resetupPin  16
#define blueLED     5
#define greenLED    4
#define redLED      14


#define IPSET_STATIC { 192, 168, 10, 16 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

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

  if (DEBUG_PRINT) {
    Serial.print("mqtt sub => ");
    Serial.print(intopic);
    Serial.print(" => ");
    Serial.println(receivedpayload);
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

  /*
  char json[] = "{\"LIGHT\":1,\"READY\":1}";
  receivedpayload.toCharArray(json, 100);
  StaticJsonBuffer<100> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    if (DEBUG_PRINT) {
      Serial.println("parseObject() failed");
    }
    subMsgReceived = HIGH;
    relayReady = LOW ;
    return;
  }

  int LIGHT = root["LIGHT"];
  int READY = root["READY"];

  if ( READY == 1 ) {
    if ( LIGHT == 1 ) {
      relaystatus = 1;
    }
    if ( LIGHT == 0) {
      relaystatus = 0;
    }
    subMsgReceived = HIGH;
    relayReady = HIGH ;
  }

  if ( READY == 0 ) {
    subMsgReceived = HIGH;
    relayReady = LOW ;
  }

  if (DEBUG_PRINT) {
    Serial.print("mqtt sub => ");
    Serial.print("ready ==> ");
    Serial.print(READY);
    Serial.print(" relaystatus => ");
    Serial.println(LIGHT);
  }
  */
}

boolean reconnect()
{
  digitalWrite(greenLED, HIGH);
  if (client.connect((char*) clientName.c_str())) {
    client.subscribe(subtopic);
    if (DEBUG_PRINT) {
      Serial.print("===> mqtt connected : ");
      Serial.println(millis() - startMills);
    }
    digitalWrite(greenLED, LOW);
  } else {
    digitalWrite(redLED, HIGH);
    if (DEBUG_PRINT) {
      Serial.print("---> mqtt failed, rc=");
      Serial.print(client.state());
      Serial.print(" ---> ");
      Serial.println(millis() - startMills);
    }
    digitalWrite(redLED, LOW);
  }
  return client.connected();
}

void wifi_connect()
{
  WiFiClient::setLocalPortStart(micros() + vdd);
  wifi_set_phy_mode(PHY_MODE_11N);
  system_phy_set_rfoption(1);
  wifi_set_channel(channel);

  if (WiFi.status() != WL_CONNECTED) {
    // WIFI
    if (DEBUG_PRINT) {
      Serial.print("===> WIFI ---> Connecting to ");
      Serial.print(millis() - startMills);
      Serial.print(" ");
      Serial.println(ssid);
    }
    delay(10);
    WiFi.mode(WIFI_STA);
    // ****************
    WiFi.begin(ssid, password);
    //WiFi.begin(ssid, password, channel, bssid);
    WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));
    // ****************

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (DEBUG_PRINT) {
        Serial.print(". ");
        Serial.print(Attempt);
      }
      //
      digitalWrite(blueLED, HIGH);
      delay(50);
      digitalWrite(blueLED, LOW);
      delay(50);
      //
      Attempt++;
      if (Attempt == 150)
      {
        if (DEBUG_PRINT) {
          Serial.println();
          Serial.println("-----> Could not connect to WIFI");
        }
        goingToSleepWithFail();
      }
    }

    wifiMills = millis() - startMills;
    if (DEBUG_PRINT) {
      Serial.println();
      Serial.print("===> WiFi connected : ");
      Serial.println(wifiMills);
      Serial.print("---> IP address: ");
      Serial.println(WiFi.localIP());

      Serial.print("===>  Current WIFI : ");
      Serial.print(WiFi.RSSI());
      Serial.print("\t");
      Serial.print(WiFi.BSSIDstr());
      Serial.print("\t");
      Serial.print(WiFi.channel());
      Serial.print("\t");
      Serial.println(WiFi.SSID());

    }

    //wifi_set_sleep_type(LIGHT_SLEEP_T);
    //wifi_set_sleep_type(MODEM_SLEEP_T);

  }
}

void setup()
{
  //
  startMills = millis();

  // pinmode setup
  pinMode(resetupPin, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  //`
  digitalWrite(resetupPin, LOW);
  digitalWrite(redLED, HIGH);

  vdd = ESP.getVcc() ;

  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }

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
    if (DEBUG_PRINT) {
      Serial.print("sub received : ready ---> ");
      Serial.println(subMills);
    }
    sendlightcmd();
  }

  if (( subMsgReceived == HIGH ) && ( relayReady == LOW )) {
    subMills = (millis() - startMills) - wifiMills ;
    if (DEBUG_PRINT) {
      Serial.print("sub received : not ready ---> ");
      Serial.println(subMills);
    }
    goingToSleepWithFail();
  }

  if ( topicMsgSent == HIGH ) {
    if (DEBUG_PRINT) {
      Serial.println("going to sleep");
    }
    goingToSleep();
  }

  if ((millis() - startMills) > 15000) {
    if (DEBUG_PRINT) {
      Serial.println("going to sleep with fail");
    }
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
  if (DEBUG_PRINT) {
    Serial.print("pub start --> ");
    Serial.println(millis() - startMills);
  }
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

    if (DEBUG_PRINT) {
      Serial.print("status pub ---> ");
      Serial.println(millis() - startMills);

    }

    delay(20);
    if (sendmqttMsg(buttontopic, buttonpayload)) {
      topicMsgSent = HIGH;
      digitalWrite(blueLED, LOW);
      if (DEBUG_PRINT) {
        Serial.print("button pub  ----> ");
        Serial.println(millis() - startMills);
      }
    }
  }
  digitalWrite(blueLED, HIGH);
}

boolean sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    if (DEBUG_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(payload);
    }

    unsigned int msg_length = payload.length();

    if (DEBUG_PRINT) {
      Serial.print(" length: ");
      Serial.println(msg_length);
    }

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      if (DEBUG_PRINT) {
        Serial.print("Publish ok  --> ");
        Serial.println(millis() - startMills);
      }
      free(p);
      return 1;
    } else {
      if (DEBUG_PRINT) {
        Serial.print("Publish failed --> ");
        Serial.println(millis() - startMills);
      }
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

  ESP.deepSleep(0);
  delay(250);
}


void goingToSleepWithFail()
{
  digitalWrite(greenLED, LOW);
  digitalWrite(blueLED, LOW);
  digitalWrite(redLED, LOW);

  digitalWrite(redLED, HIGH);
  delay(300);
  digitalWrite(redLED, LOW);
  delay(300);
  digitalWrite(redLED, HIGH);
  delay(300);
  digitalWrite(redLED, LOW);

  ESP.deepSleep(0);
  delay(250);
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
