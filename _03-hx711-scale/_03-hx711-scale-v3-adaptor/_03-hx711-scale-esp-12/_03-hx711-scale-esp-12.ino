#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>  /

// wifi
#ifdef __IS_MY_HOME
  #include "/usr/local/src/ap_setting.h"
#else
  #include "ap_setting.h"
#endif

#define nemoisOnPin 14

char* topicEvery   = "esp8266/arduino/s16";
char* topicAverage = "esp8266/arduino/s06";


char* hellotopic = "HELLO";

String clientName;
WiFiClient wifiClient;

IPAddress server(192, 168, 10, 10);
PubSubClient client(wifiClient, server);

int inuse = LOW;

void callback(const MQTT::Publish& pub) {
}

void setup() {
  Serial.begin(38400);
  //Wire.pins(4, 5);
  Wire.begin(4,5);
  Serial.println("HX711 START");
  delay(20);

  pinMode(nemoisOnPin,INPUT);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  client.set_callback(callback);

  WiFi.mode(WIFI_STA);

  #ifdef __IS_MY_HOME
  WiFi.begin(ssid, password, channel, bssid);
  WiFi.config(IPAddress(192, 168, 10, 16), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
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
    Serial.println(topicEvery);

    if (client.publish(hellotopic, "hello from ESP8266 s06")) {
      Serial.println("Publish ok");
    } else {
      Serial.println("Publish failed");
    }

  } else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }

}

void loop() {
  inuse = digitalRead(nemoisOnPin);
  if ( inuse == HIGH ) {
    requestHx711();
  } 
  client.loop();
  delay(500);
}

void requestHx711() {
     Wire.requestFrom(2, 3); 

     int x;
     byte a, b, c;

     a = Wire.read();
     b = Wire.read();
     c = Wire.read();

     x = a;
     x = x << 8 | b;

     if ( c == 0 ) {
       Serial.print("Raw Signal Value: ");
       Serial.print(x);


       String payload = "{\"NemoWeight\":";
       payload += x;
       payload += "}";

      sendHx711topicEvery(payload);
    }

     if ( c == 1 ) {
       Serial.print("Raw Signal Value: ");
       Serial.print(x);


       String payload = "{\"WeightAvg\":";
       payload += x;
       payload += "}";

      sendHx711topicAverage(payload);
    }

}


void sendHx711topicEvery(String payload) {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again HX711");
      Serial.print("Topic is: ");
      Serial.println(topicEvery);
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
      client.publish(MQTT::Publish(topicEvery, (char*) payload.c_str())
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


void sendHx711topicAverage(String payload) {
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again HX711");
      Serial.print("Topic is: ");
      Serial.println(topicAverage);
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
      client.publish(MQTT::Publish(topicAverage, (char*) payload.c_str())
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

