
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "/usr/local/src/ap_setting.h"


char* topic   = "/30/Status/MQTT";
char* subtopic = "inTopic";
char* hellotopic = "HELLO";

IPAddress server(192, 168, 10, 138);
WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);

long lastMsg = 0;
char msg[50];
int value = 0;
long lastReconnectAttempt = 0;
int resetCountforMqttReconnect = 0;

void setup() {
  Serial.begin(115200);
  setup_wifi();
}

void setup_wifi() {
  delay(20);
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
      delay(2000);
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {

}

boolean reconnect() {
  if (!client.connected()) {
    if (client.connect("arduinoClient")) {
      client.publish(hellotopic, "hello world");
      client.subscribe(subtopic);
    }
  }
  return client.connected();
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 1000) {
        Serial.print("failed, rc=");
        Serial.println(client.state());
        Serial.println("MQTT disconnected....");
        lastReconnectAttempt = now;
        resetCountforMqttReconnect++;
        if ( resetCountforMqttReconnect == 100 ) {
          Serial.println();
          Serial.println("Could not connect to mqtt");
          ESP.restart();
          delay(2000);
        }
        if (reconnect()) {
          Serial.println("MQTT connected....");
          lastReconnectAttempt = 0;
          resetCountforMqttReconnect = 0;
        }
      }
    }  else {
      long now = millis();
      if (now - lastMsg > 30000) {
        lastMsg = now;
        Serial.println(client.state());
        client.publish(topic, "on");
      }
      client.loop();
    }
  } else {
    setup_wifi();
  }
}
