#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>  

/*
extern "C" {
#include "user_interface.h"
}

extern "C" uint16_t readvdd33(void);

*/

ADC_MODE(ADC_VCC);

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

char* topic = "esp8266/arduino/s06";
char* hellotopic = "HELLO";

String clientName;
WiFiClient wifiClient;
String payload;

long startMills ;


IPAddress server(192, 168, 10, 10);
PubSubClient client(wifiClient, server);

int vdd ;
int msgReceived = LOW ;

void setup() 
{
  Serial.begin(38400);
  startMills = millis();
  //Wire.pins(4, 5);
  delay(200);
  Wire.begin(4, 5);
  Serial.println("pet pad scale started");

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

#ifdef __IS_MY_HOME
  WiFi.config(IPAddress(192, 168, 10, 16), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
#endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
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

  Serial.println(millis() - startMills);

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);

  // vdd = readvdd33();
  vdd = ESP.getVcc() ; 
  
}


void loop() 
{
  requestHx711();
  delay(200);

  if ( msgReceived == HIGH )
  {
    sendHx711(payload);
  }
  delay(200);
  
  Serial.println(millis() - startMills);
  Serial.println("going to sleep");

  delay(200);
  ESP.deepSleep(0);
  delay(200);
}

void requestHx711() 
{
  Wire.requestFrom(2, 4);

  int x, y;
  byte a, b, c, d;

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();
  d = Wire.read();

  x = a;
  x = x << 8 | b;

  y = c;
  y = y << 8 | d;

  Serial.print("Raw Signal Value: ");
  Serial.println(x);

  Serial.print("VDD Signal Value: ");
  Serial.println(y);


  if ( x >= 10000 ) {
    Serial.println("scale is sleeping");
    return;
  } else {

    payload = "{\"NemoWeight\":";
    payload += x;
    payload += ",\"vdd\":";
    payload += vdd;
    payload += ",\"pro\":";
    payload += y;
    payload += "}";

    msgReceived = HIGH ;

  }

}

void sendMsgSentSig() 
{
  int x = 1;
  Wire.beginTransmission(2);
  Wire.write(x);
  Wire.endTransmission();
}

void sendHx711(String payload) 
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

    if (client.publish(hellotopic, "hello from esp8266/arduino/s06")) {
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

    if (
      client.publish(MQTT::Publish(topic, (char*) payload.c_str())
                     .set_retain()
                    )
    ) {
      Serial.println("Publish ok");
      Serial.println(millis() - startMills);
      client.disconnect();
      sendMsgSentSig();
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

