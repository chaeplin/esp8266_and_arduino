#include <PubSubClient.h>
#include <ESP8266WiFi.h>
// https://github.com/openenergymonitor/EmonLib
#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

// mqtt
char* topic = "esp8266/arduino/s07";
char* doortopic = "esp8266/arduino/s05" ;
char* hellotopic = "HELLO";
IPAddress server(192, 168, 10, 10);

// pin : using line tracker
#define IRPIN 4
#define DOORPIN 5

#define REPORT_INTERVAL 60000 // in msec


volatile long startMills ;
volatile long revMills ;

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


const int numReadings = 10;

int readings[numReadings];      // the readings from the analog input
int indexof = 0;                  // the indexof of the current reading
int total = 0;                  // the running total
int average = 0;                // the average


void callback(const MQTT::Publish& pub) {
}

WiFiClient wifiClient;
PubSubClient client(wifiClient, server);

void setup() {
  Serial.begin(38400);
  Serial.println("power meter test!");
  delay(20);

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
    if (Attempt == 100)
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

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);

  if (client.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topic);

    if (client.publish(hellotopic, "hello from esp8266/arduino/s07")) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }

  startMills = millis();
  sentMills = millis();
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


  emon1.current(A0, 75);  // Current: input pin, calibration.

  oldirStatus = LOW ;

  for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0 ;

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
  total = total - readings[indexof];

  VIrms = emon1.calcIrms(1480) * 220.0 ;
  readings[indexof] = VIrms ;

  total = total + readings[indexof];
  indexof = indexof + 1;
  if (indexof >= numReadings)
    indexof = 0;

  average = total / numReadings;

/*
  Serial.print("revMills ==> ");
  Serial.print(revMills);  
  Serial.print(" oldrevMills ==> ");
  Serial.println(oldrevMills);  
*/

  if ( revMills > 600 ) {
      revValue = float(( 3600  * 1000 ) / ( 600 * float(revMills) ) ) * 1000 ;
  }

/*
  Serial.print("revValue ==> ");
  Serial.println(revValue);  
*/
  
  if ( oldrevValue == 0 ) {
    oldrevValue = revValue ;
  }

  if ( oldrevMills == 0 ) {
    oldrevMills = revMills ;
  }

    payload = "{\"VIrms\":";
    payload += average;
    payload += ",\"revValue\":";
    payload += ( revValue + oldrevValue ) / 2 ;
    payload += ",\"revMills\":";
    payload += ( revMills + oldrevMills ) / 2 ;
    payload += ",\"powerAvg\":";
    payload += ( average + ((revValue + oldrevValue ) / 2 )) / 2 ;
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

        sendDoormqttMsg(doorpayload);
        olddoorStatus = doorStatus ;
  }


  if (( irStatus != oldirStatus ) && ( revMills > 600 )) {
    sendmqttMsg(payload);
    sentMills = millis();
    oldirStatus = irStatus ;
    oldrevValue = revValue ;
    oldrevMills = revMills ;
  }

  if (((millis() - sentMills) > REPORT_INTERVAL ) && ( revMills > 600 )) {
    sendmqttMsg(payload);
    sentMills = millis();
  }

  if ( revMills > 1000 ) {
    delay(int(float(revMills)/10));
  }else {
    delay(100);
  }
}

void sendDoormqttMsg(String doorpayload)
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again esp8266/arduino/s07");
      Serial.print("Topic is: ");
      Serial.println(doortopic);
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(doorpayload);

   if (client.publish(doortopic, (char*) doorpayload.c_str())) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }
}

void sendmqttMsg(String payload)
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again esp8266/arduino/s07");
      Serial.print("Topic is: ");
      Serial.println(topic);
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

   if (client.publish(topic, (char*) payload.c_str())) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
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
