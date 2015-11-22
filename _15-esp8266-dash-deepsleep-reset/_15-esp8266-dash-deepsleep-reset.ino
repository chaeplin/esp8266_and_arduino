#include <ESP8266WiFi.h>
#include <PubSubClient.h>

extern "C" {
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define DEBUG_PRINT 1
#define EVENT_PRINT 1

#include "/usr/local/src/ap_setting.h"

#define resetupPin  16
#define blueLED     5
#define greenLED    4
#define redLED      2

char* topic = "esp8266/cmd/light";
char* subtopic = "esp8266/cmd/light/rlst";

String clientName;
String payload;

int subMsgReceived = LOW;
int topicMsgSent   = LOW;

volatile int relaystatus    = LOW;

IPAddress server(192, 168, 10, 10);
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

long lastReconnectAttempt = 0;

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  if (EVENT_PRINT) {
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

  if (EVENT_PRINT) {
    Serial.print("");
    Serial.print(" => relaystatus => ");
    Serial.println(relaystatus);
  }

  subMsgReceived = HIGH;

}

boolean reconnect()
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      client.subscribe(subtopic);
      if (EVENT_PRINT) {
        Serial.println("connected");
      }
    } else {
      if (EVENT_PRINT) {
        Serial.print("failed, rc=");
        Serial.println(client.state());
      }
    }
  }
  return client.connected();
}

void wifi_connect()
{
  // WIFI
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  delay(200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(192, 168, 10, 16), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    //
    digitalWrite(blueLED, HIGH);
    delay(50);
    digitalWrite(blueLED, LOW);
    delay(50);
    //
    Attempt++;
    Serial.print(". ");
    Serial.print(Attempt);
    if (Attempt == 150)
    {
      Serial.println();
      Serial.println("------------------------------> Could not connect to WIFI");
      ESP.restart();
      delay(200);
    }
  }

  Serial.println();
  Serial.println("===> WiFi connected");
  Serial.print("---------------------------------> IP address: ");
  Serial.println(WiFi.localIP());

  //wifi_set_sleep_type(LIGHT_SLEEP_T);
  //wifi_set_sleep_type(MODEM_SLEEP_T);
  //
}

void setup()
{
  // pinmode setup
  pinMode(resetupPin, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(greenLED, OUTPUT);
  pinMode(redLED, OUTPUT);

  digitalWrite(resetupPin, LOW);

  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;

  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      client.subscribe(subtopic);
      if (EVENT_PRINT) {
        Serial.println("connected");
      }
      digitalWrite(greenLED, HIGH);
      delay(100);
      digitalWrite(greenLED, LOW);
    } else {
      if (EVENT_PRINT) {
        Serial.print("failed, rc=");
        Serial.println(client.state());
      }
    }
  }

}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
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

  if ( subMsgReceived == HIGH ) {
    digitalWrite(greenLED, HIGH);
    delay(200);
    digitalWrite(greenLED, LOW);
    sendlightcmd();
  }

  if ( topicMsgSent == HIGH ) {
    digitalWrite(greenLED, HIGH);
    delay(300);
    digitalWrite(greenLED, LOW);
    goingToSleep();
  }

  client.loop();
}

void sendlightcmd()
{
  String lightpayload = "{\"LIGHT\":";
  lightpayload += ! relaystatus;
  lightpayload += "}";

  if (sendmqttMsg(topic, lightpayload)) {
    topicMsgSent = HIGH;
    client.disconnect();
  }
}

boolean sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    if (EVENT_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(payload);
    }

    unsigned int msg_length = payload.length();

    if (EVENT_PRINT) {
      Serial.print(" length: ");
      Serial.println(msg_length);
    }

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      if (EVENT_PRINT) {
        Serial.println("Publish ok");
      }
      free(p);
      return 1;
    } else {
      if (EVENT_PRINT) {
        Serial.println("Publish failed");
      }
      free(p);
      return 0;
    }
  }
}

void goingToSleep()
{
  Serial.println("Going to sleep");
  delay(250);
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
