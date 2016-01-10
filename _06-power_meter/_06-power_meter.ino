#include <PubSubClient.h>
#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>
//#include <ArduinoOTA.h>
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

#define DEBUG_PRINT 0

//#define IPSET_STATIC { 192, 168, 10, 16 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

// ****************
void sendmqttMsg(char* topictosend, String payloadtosend);
void callback(char* intopic, byte* inpayload, unsigned int length);
String macToStr(const uint8_t* mac);
void IRCHECKING_START();
void DOORCHECKING();
void count_powermeter();

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
//int32_t channel = WIFI_CHANNEL;
//byte bssid[] = WIFI_BSSID;
byte mqtt_server[] = MQTT_SERVER;
//
//byte ip_static[] = IPSET_STATIC;
byte ip_gateway[] = IPSET_GATEWAY;
byte ip_subnet[] = IPSET_SUBNET;
byte ip_dns[] = IPSET_DNS;
// ****************


// ********** change MQTT_KEEPALIVE to 60 at PubSubClient.h *****************
EnergyMonitor emon1;
Average<float> ave(10);

// mqtt
char* topic = "esp8266/arduino/s07";
char* doortopic = "esp8266/arduino/s05" ;
char* hellotopic = "HELLO";

char* willTopic = "clients/power";
char* willMessage = "0";

char* subtopic = "esp8266/check";

IPAddress server(192, 168, 10, 10);

// pin : using line tracker
#define IRPIN 4
#define DOORPIN 5

//#define REPORT_INTERVAL 60000 // in msec
#define REPORT_INTERVAL 1000 // in msec

volatile unsigned long startMills ;
volatile unsigned long revMills ;

unsigned long timemillis;

unsigned long oldrevMills ;
unsigned long sentMills ;
volatile unsigned long revCounts ;

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

// send reset info
String getResetInfo ;
int ResetInfo = LOW;

int average = 0;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

long lastReconnectAttempt = 0;
long calcIrmsmillis = 0;

void wifi_connect() {
  // WIFI
  if (DEBUG_PRINT) {
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (DEBUG_PRINT) {
      Serial.print(".");
    }
    if (Attempt == 200)
    {
      if (DEBUG_PRINT) {
        Serial.println();
        Serial.println("Could not connect to WIFI");
      }
      ESP.restart();
    }
  }
  if (DEBUG_PRINT) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

boolean reconnect() {
  //if (!client.connected()) {
  if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
    client.publish(willTopic, "1", true);
    if ( ResetInfo == LOW) {
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      ResetInfo = HIGH;
    } else {
      client.publish(hellotopic, "hello again 1 from ESP8266 s07");
    }
    client.subscribe(subtopic);
    if (DEBUG_PRINT) {
      Serial.println("---------------> connected");
    }
  } else {
    if (DEBUG_PRINT) {
      Serial.print("----------------> failed, rc=");
      Serial.println(client.state());
    }
  }
  //}
  //timemillis = millis();
  return client.connected();
}

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  if (DEBUG_PRINT) {
    Serial.print(intopic);
    Serial.print(" => ");
    Serial.println(receivedpayload);
  }

  if ( receivedpayload == "{\"DOOR\":\"CHECKING\"}") {

    String check_doorpayload = "{\"DOOR\":";
    if ( doorStatus == 0 ) {
      check_doorpayload += "\"CHECK_CLOSED\"";
    }
    else {
      check_doorpayload += "\"CHECK_OPEN\"";
    }
    check_doorpayload += "}";
    sendmqttMsg(doortopic, check_doorpayload);
  }

}

void setup() {
  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }
  delay(20);
  if (DEBUG_PRINT) {
    Serial.println("power meter test!");

    Serial.print("ESP.getChipId() : ");
    Serial.println(ESP.getChipId());

    Serial.print("ESP.getFlashChipId() : ");
    Serial.println(ESP.getFlashChipId());

    Serial.print("ESP.getFlashChipSize() : ");
    Serial.println(ESP.getFlashChipSize());
  }

  delay(5000);
  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);


  client.setServer(server, 1883);

  lastReconnectAttempt = 0;
  revCounts = 0 ;

  getResetInfo = "hello from ESP8266 s07 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

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

  /*
    //OTA
    // Port defaults to 8266
    //ArduinoOTA.setPort(8266);

    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname("esp-power");

    // No authentication by default
    // ArduinoOTA.setPassword((const char *)"123");

    ArduinoOTA.onStart([]() {
    Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
  */
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
      if (DEBUG_PRINT) {
        Serial.print("------------> failed, rc=");
        Serial.print(client.state());
      }
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 1000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    }
  } else {
    wifi_connect();
  }

  if ((millis() - sentMills) > ( REPORT_INTERVAL / 2) ) {
    unsigned long emonmillis = millis();
    VIrms = emon1.calcIrms(1480) * 220.0;
    calcIrmsmillis = millis() - emonmillis;

    ave.push(VIrms);
    average = ave.mean();
  }

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
  payload += ",\"calcIrmsmillis\":";
  payload += calcIrmsmillis;
  payload += ",\"revCounts\":";
  payload += revCounts;
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

  if (((millis() - sentMills) > REPORT_INTERVAL ) && ( irStatus == oldirStatus ) &&  ( revMills > 600 )) {
    sendmqttMsg(topic, payload);
    sentMills = millis();
  }

  if (( irStatus != oldirStatus ) && ( revMills > 600 )) {
    sendmqttMsg(topic, payload);
    sentMills = millis();
    oldirStatus = irStatus ;
    oldrevValue = revValue ;
    oldrevMills = revMills ;
  }

  //ArduinoOTA.handle();
  client.loop();
  //delay(500);
}

void sendmqttMsg(char* topictosend, String payloadtosend)
{

  if (client.connected()) {
    if (DEBUG_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(payloadtosend);
    }
    unsigned int msg_length = payloadtosend.length();
    if (DEBUG_PRINT) {
      Serial.print(" length: ");
      Serial.println(msg_length);
    }
    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payloadtosend.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      if (DEBUG_PRINT) {
        Serial.println("Publish ok");
      }
      free(p);
    } else {
      if (DEBUG_PRINT) {
        Serial.println("Publish failed");
      }
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

  /*
    if (( millis() - startMills ) < ( revMills / 5 )) {
    return;
    } else {
    revMills   = (millis() - startMills)  ;
    startMills = millis();
    irStatus   = !irStatus ;
    revCounts++;
    }
  */

  revMills   = (millis() - startMills)  ;
  startMills = millis();
  irStatus   = !irStatus ;
  revCounts++;

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
