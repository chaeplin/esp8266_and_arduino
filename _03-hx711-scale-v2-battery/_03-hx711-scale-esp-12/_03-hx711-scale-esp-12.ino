#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>  /


extern "C"{
  #include "user_interface.h"
}

extern "C" uint16_t readvdd33(void);


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

IPAddress server(192, 168, 10, 10);
PubSubClient client(wifiClient, server);

int msgsentPin = 12 ; // espRfStatePin
int vdd ;

void setup() {
  Serial.begin(38400);
  //Wire.pins(4, 5);
  Wire.begin(4,5);
  Serial.println("HX711 START");

  pinMode(msgsentPin, OUTPUT);

  digitalWrite(msgsentPin, HIGH);
  delay(20);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);


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

  Serial.println(millis() - startMills);

  requestHx711();
  sendHx711(payload);

}


void loop() {
  delay(100);
  Serial.println(millis() - startMills);
  Serial.println("going to sleep");
  ESP.deepSleep(0);
}



void requestHx711() {
     Wire.requestFrom(2, 2); 

     int x;
     byte a, b;

     a = Wire.read();
     b = Wire.read();

     x = a;
     x = x << 8 | b;

     Serial.print("Raw Signal Value: ");
     Serial.print(x);

     vdd = readvdd33();

     payload = "{\"NemoWeight\":";
     payload += x;
     payload += ",\"vdd\":";
     payload += vdd;
     payload += "}";

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

