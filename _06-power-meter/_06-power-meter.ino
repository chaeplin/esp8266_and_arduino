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
char* hellotopic = "HELLO";
IPAddress server(192, 168, 10, 10);

// pin : using line tracker
#define IRPIN 4

#define REPORT_INTERVAL 20000 // in msec


volatile long startMills ;
volatile float revMills ;
long sentMills ;

volatile int IRSTATUS = LOW ;
int OLDIRSTATUS ;

float revValue ;
double VIrms ;

//
String clientName ;
String payload ;

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

  #ifdef __IS_MY_HOME
  WiFi.begin(ssid, password, channel, bssid);
  WiFi.config(IPAddress(192, 168, 10, 17), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
  #else
  WiFi.begin(ssid, password); 
  #endif


  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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

  pinMode(IRPIN, INPUT);
  attachInterrupt(4, IRCHECKING_START, RISING); 

  emon1.current(A0, 74);  // Current: input pin, calibration.

  OLDIRSTATUS = LOW ;

}

void IRCHECKING_START(){
  detachInterrupt(4);
  attachInterrupt(4, count_powermeter, RISING);
  startMills = millis();
}

void loop()
{
  
  VIrms = emon1.calcIrms(1480) * 220.0 ; 
  revValue = (( 3600  * 1000 )/ ( 600 * revMills ) ) * 1000 ;

  payload = "{\"VIrms\":";
  payload += VIrms;
  payload += ",\"revValue\":";
  payload += revValue;
  payload += ",\"revMills\":";
  payload += revMills;
  payload += "}";

  /*
  if ( revValue > 0 ) {
    Serial.print("power => ");
    Serial.print(VIrms); 
    Serial.print(" ir => ");
    Serial.print(revMills);
    Serial.print(" W => ");
    Serial.println(revValue);

  }
  */

  if ( ( revMills != 0.00 ) && ( IRSTATUS != OLDIRSTATUS ) ) {
       sendmqttMsg(payload);
       OLDIRSTATUS = IRSTATUS;
       sentMills = millis();
  }

  if ((millis() - sentMills) > REPORT_INTERVAL ) {
       sendmqttMsg(payload);
       sentMills = millis();
  }

  delay(1000);

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

    if (
      client.publish(MQTT::Publish(topic, (char*) payload.c_str())
                .set_retain()
               )
      ) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }
  }
}


void count_powermeter()
{
// if (( millis() - startMills ) < 600 ) {
  if (( millis() - startMills ) < ( revMills / 3 )) {
       return;
  } else {
    revMills   = (millis() - startMills)  ;
    startMills = millis();
    IRSTATUS   = !IRSTATUS ;
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
