#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define DEBUG_PRINT 0

#define resetupPin  16
#define blueLED     5
#define greenLED    4
#define redLED      14

// #define MQTT_KEEPALIVE 3 in PubSubClient.h

char* topic = "esp8266/cmd/light";
char* subtopic = "esp8266/cmd/light/rlst";
char* buttontopic = "button";

String clientName;
String payload;

volatile int subMsgReceived = LOW;
volatile int topicMsgSent   = LOW;
volatile int relaystatus    = LOW;

int vdd;

//
unsigned long startMills = 0;
unsigned long wifiMills = 0;
unsigned long subMills = 0;

IPAddress server(192, 168, 10, 10);
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

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

  if ( receivedpayload == "{\"LIGHT\":1}") {
    relaystatus = 1 ;
  }
  else if ( receivedpayload == "{\"LIGHT\":0}") {
    relaystatus = 0 ;
  }

  if (DEBUG_PRINT) {
    Serial.print("mqtt sub => ");
    Serial.print("relaystatus => ");
    Serial.println(relaystatus);
  }
  subMsgReceived = HIGH;
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
    WiFi.begin(ssid, password);
    WiFi.config(IPAddress(192, 168, 10, 16), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));

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
    }

    //wifi_set_sleep_type(LIGHT_SLEEP_T);
    //wifi_set_sleep_type(MODEM_SLEEP_T);
    //
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

  system_deep_sleep_set_option(2);
  wifi_set_phy_mode(PHY_MODE_11N);

  digitalWrite(redLED, LOW);
  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;
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
    } else {
      client.loop();
    }
  } else {
    wifi_connect();
  }

  if ( subMsgReceived == HIGH ) {
    subMills = (millis() - startMills) - wifiMills ;
    if (DEBUG_PRINT) {
      Serial.print("sub received ---> ");
      Serial.println(subMills);
    }
    sendlightcmd();
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
  yield();
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
