/*

// pro mini
5 - mini 7 : input - door status
16 - mini 8 : output - wifi/mqtt status
rest - mini 9 : external reset

* to program esp8266 with serial of esp8266, 
* press reset button of promini till uploading is finished

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/*
extern "C"{
  #include "user_interface.h"
}


extern "C" uint16_t readvdd33(void);
*/

#ifdef __IS_MY_HOME
  #include "/usr/local/src/ap_setting.h"
#else
  #include "ap_setting.h"
#endif

ADC_MODE(ADC_VCC);

int msgsentPin = 16 ; // espRfStatePin
int doorstatusPin = 5 ; // espDoorPin

int doorstatus ;
int vdd ;

long startMills ;

char* topic = "esp8266/arduino/s05" ;
char* hellotopic = "HELLO" ;

// mqtt server
IPAddress server(192, 168, 10, 10);

String clientName;
WiFiClient wifiClient;

void callback(const MQTT::Publish& pub) {
  // handle message arrived
}

PubSubClient client(wifiClient, server);

void setup()
{
  Serial.begin(38400);

  startMills = millis();

  Serial.println("Starting door alarm"); 
  pinMode(msgsentPin, OUTPUT);
  pinMode(doorstatusPin, INPUT);

  digitalWrite(msgsentPin, HIGH);
  delay(20);

  Serial.println(millis() - startMills);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

 // client.set_callback(callback);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 

  #ifdef __IS_MY_HOME
  WiFi.config(IPAddress(192, 168, 10, 14), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
  #endif
  
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
  
  Serial.println(millis() - startMills);
  
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

  doorstatus = digitalRead(doorstatusPin) ;
  
  Serial.println(millis() - startMills);
  
  Serial.print("doorstatus  : ");
  Serial.println(doorstatus);

  //vdd = readvdd33();

  vdd = ESP.getVcc() ; 

  
  String payload = "{\"DOOR\":";

  if ( doorstatus == 0 ) {
          payload += "\"CLOSED\"";
  } 
  else {
          payload += "\"OPEN\"";
  }

  /*
  payload += doorstatus;
  */

  payload += ",\"vdd\":";
  payload += vdd;
  payload += "}";  
       
  sendDoorAlarm(payload);
  
}

void loop()
{
  delay(100);
  Serial.println(millis() - startMills);
  Serial.println("going to sleep");
  ESP.deepSleep(0);
}


void sendDoorAlarm(String payload) 
{

  if (
        client.connect(MQTT::Connect((char*) clientName.c_str())
                .set_clean_session()
                .set_will("status", "down")
                .set_keepalive(2))
    ) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topic);

    if (client.publish(hellotopic, "hello from esp8266/arduino/s05")) {
      Serial.println("Hello Publish ok");
    }
    else {
      Serial.println("Hello Publish failed");
    }

  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }


  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (client.publish(topic, (char*) payload.c_str())) {
      Serial.println("Publish ok");
      Serial.println(millis() - startMills);
      client.disconnect();
      Serial.println("set msgsentPin to LOW");
      digitalWrite(msgsentPin, LOW);
    }
    else {
      Serial.println("Publish failed");
      abort();
    }
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

//---------------


