#include <PubSubClient.h>
#include <ESP8266WiFi.h>
// https://github.com/openenergymonitor/EmonLib
#include "EmonLib.h" // Include Emon Library
#include <Average.h>

#define _IS_MY_HOME
// WIFI
#ifdef _IS_MY_HOME
//#include "/usr/local/src/ap_settingii.h"
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

//
EnergyMonitor emon1;                   // Create an instance
Average<float> ave(10);

// mqtt
char* topic = "esp8266/arduino/s07";
char* doortopic = "esp8266/arduino/s05" ;
char* hellotopic = "HELLO";

char* willTopic = "clients/power";
char* willMessage = "0";

IPAddress server(192, 168, 10, 10);

// pin : using line tracker
#define IRPIN 4
#define DOORPIN 5

//#define REPORT_INTERVAL 60000 // in msec
#define REPORT_INTERVAL 3000 // in msec

volatile long startMills ;
volatile long revMills ;

unsigned long timemillis;

long oldrevMills ;
long sentMills ;

volatile int irStatus = LOW ;

int oldirStatus ;

float revValue ;
float oldrevValue ;
double VIrms ;

// door
volatile int doorStatus ;
int olddoorStatus ;

//
String clientName ;
String payload ;
String doorpayload ;

int average = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);

void callback(char* topic, byte* payload, unsigned int length) {
}

long lastReconnectAttempt = 0;

void wifi_connect() {
  // WIFI
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    Serial.print(".");
    if (Attempt == 200)
    {
      Serial.println();
      Serial.println("Could not connect to WIFI");
      ESP.restart();
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
/*
  timemillis = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(willTopic, "1", true);
        client.publish(hellotopic, "hello again wifi and mqtt from ESP8266 s07");
        Serial.println("reconnecting wifi and mqtt");
      } else {
        Serial.println("mqtt publish fail after wifi reconnect");
      }
    } else {
      client.publish(willTopic, "1", true);
      client.publish(hellotopic, "hello again wifi from ESP8266 s07");
    }
  }
*/
}

boolean reconnect() {
  if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
    client.publish(willTopic, "1", true);
    client.publish(hellotopic, "hello again 1 from ESP8266 s07");
    Serial.println("---------------> connected");
  } else {
    Serial.print("----------------> failed, rc=");
    Serial.println(client.state());
  }
  //timemillis = millis();
  return client.connected();
}

void setup() {
  Serial.begin(38400);
  delay(20);
  Serial.println("power meter test!");
  
  Serial.print("ESP.getChipId() : ");
  Serial.println(ESP.getChipId());

  Serial.print("ESP.getFlashChipId() : ");
  Serial.println(ESP.getFlashChipId());
  
  Serial.print("ESP.getFlashChipSize() : ");
  Serial.println(ESP.getFlashChipSize());
  delay(20);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);


  client.setServer(server, 1883);

  lastReconnectAttempt = 0;

  String getResetInfo = "hello from ESP8266 s07 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
        client.publish(willTopic, "1", true);
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        Serial.print("Sending payload: ");
        Serial.println(getResetInfo);
      }
    } else {
      client.publish(willTopic, "1", true);
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      Serial.print("Sending payload: ");
      Serial.println(getResetInfo);
    }
  }

  startMills = millis();
  sentMills = millis();
  timemillis = millis();
  revMills  = 0 ;
  revValue  = 0 ;
  oldrevMills = 0 ;
  oldrevValue = 0 ;

  pinMode(IRPIN, INPUT);
  pinMode(DOORPIN, INPUT_PULLUP);

  doorStatus = digitalRead(DOORPIN);
  olddoorStatus = doorStatus;

  attachInterrupt(4, IRCHECKING_START, RISING);
  attachInterrupt(5, DOORCHECKING, CHANGE);


  emon1.current(A0, 75);

  oldirStatus = LOW ;

}

void DOORCHECKING() {
  doorStatus = digitalRead(DOORPIN);
}

void IRCHECKING_START() {
  detachInterrupt(4);
  attachInterrupt(4, count_powermeter, RISING);
  startMills = millis();
  oldirStatus = HIGH ;
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.print("------------> failed, rc=");
      Serial.print(client.state());
      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } /* else {
      client.loop();
    } */
  } else {
    wifi_connect();
  }

  VIrms = emon1.calcIrms(1480) * 220.0;
  ave.push(VIrms);
  average = ave.mean();

  if ( revMills > 600 ) {
    revValue = (float(( 3600  * 1000 ) / ( 600 * float(revMills) ) ) * 1000);
  }

  if ( oldrevValue == 0 ) {
    oldrevValue = revValue;
  }

  if ( oldrevMills == 0 ) {
    oldrevMills = revMills;
  }

  payload = "{\"VIrms\":";
  payload += average;
  payload += ",\"revValue\":";
  payload += revValue;
  payload += ",\"revMills\":";
  payload += revMills;
  payload += ",\"powerAvg\":";
  payload += (( average + revValue ) / 2) ;
  payload += ",\"Stddev\":";
  payload += ave.stddev();
  payload += ",\"FreeHeap\":";
  payload += ESP.getFreeHeap();
  payload += ",\"RSSI\":";
  payload += WiFi.RSSI();
  payload += ",\"millis\":";
  payload += (millis() - timemillis);
  payload += "}";

  if ( doorStatus != olddoorStatus ) {

    doorpayload = "{\"DOOR\":";

    if ( doorStatus == 0 ) {
      doorpayload += "\"CLOSED\"";
    }
    else {
      doorpayload += "\"OPEN\"";
    }
    doorpayload += "}";

    sendmqttMsg(doortopic, doorpayload);
    olddoorStatus = doorStatus ;
  }

  if (( irStatus != oldirStatus ) && ( revMills > 600 )) {
    sendmqttMsg(topic, payload);
    sentMills = millis();
    oldirStatus = irStatus ;
    oldrevValue = revValue ;
    oldrevMills = revMills ;
  }

  if (((millis() - sentMills) > REPORT_INTERVAL ) && ( revMills > 600 )) {
    sendmqttMsg(topic, payload);
    sentMills = millis();
  }
  client.loop();
  delay(50);

}


void sendmqttMsg(char* topictosend, String payloadtosend)
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
      client.publish(willTopic, "1", true);
      client.publish(hellotopic, "hello again 2 from ESP8266 s07");
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.print(payloadtosend);

    unsigned int msg_length = payloadtosend.length();

    Serial.print(" length: ");
    Serial.println(msg_length);

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payloadtosend.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      Serial.println("Publish ok");
      free(p);
    } else {
      Serial.println("Publish failed");
      free(p);
    }
  }
}


void count_powermeter()
{
  // 600 rev/kWh --> 1 rev 6 sec 1kW
  // max A : 40
  // 220 V * 45A = 9900,
  // 0.6sec -> 10kW
  //

  if (( millis() - startMills ) < 600 ) {
    return;
  }

  if (( millis() - startMills ) < ( revMills / 3 )) {
    return;
  } else {
    revMills   = (millis() - startMills)  ;
    startMills = millis();
    irStatus   = !irStatus ;
  }
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
