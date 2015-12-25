#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>

#define _IS_MY_HOME

// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

extern "C" {
#include "user_interface.h"
}

ADC_MODE(ADC_VCC);

#define DEBUG_PRINT 0

#define dsout 5
#define ONE_WIRE_BUS 4
#define TEMPERATURE_PRECISION 9

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer, outsideThermometer;

#define IPSET_STATIC { 192, 168, 10, 21 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
int32_t channel = WIFI_CHANNEL;
//byte bssid[] = WIFI_BSSID;
byte mqtt_server[] = MQTT_SERVER;
//
byte ip_static[] = IPSET_STATIC;
byte ip_gateway[] = IPSET_GATEWAY;
byte ip_subnet[] = IPSET_SUBNET;
byte ip_dns[] = IPSET_DNS;
// ****************

char* topic = "esp8266/arduino/s04";
char* hellotopic = "HELLO";

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);

long lastReconnectAttempt = 0;

unsigned long startMills = 0;
unsigned long wifiMills = 0;

String clientName;

  float tempCoutside ;
  float tempCinside ;

int vdd;

void callback(char* intopic, byte* inpayload, unsigned int length) {
  //
}

boolean reconnect()
{
  if (client.connect((char*) clientName.c_str())) {
    //
  } else {
    goingToSleep();
  }
  return client.connected();
}

void wifi_connect()
{
  WiFiClient::setLocalPortStart(micros() + vdd);
  wifi_set_phy_mode(PHY_MODE_11N);
  system_phy_set_rfoption(1);
  wifi_set_channel(channel);

  if (WiFi.status() != WL_CONNECTED) {
    delay(10);
    WiFi.mode(WIFI_STA);
    // ****************
    WiFi.begin(ssid, password);
    //WiFi.begin(ssid, password, channel, bssid);
    WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));
    // ****************

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        goingToSleep();
      }
    }

    wifiMills = millis() - startMills;
  }
}


void setup(void)
{
  startMills = millis();
  // start serial port
  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }
  delay(10);
  vdd = ESP.getVcc();

  // get ds18b20 temp
  delay(10);
  pinMode(dsout, OUTPUT);
  digitalWrite(dsout, HIGH);
  delay(10);

  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) {
    goingToSleep();
  }
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  tempCoutside = tempCinside = sensors.getTempC(insideThermometer);

  digitalWrite(dsout, LOW);

  if ( isnan(tempCinside) || isnan(tempCoutside) || isnan(vdd) ) {
    goingToSleep();
  }

  //
  wifi_connect();

  //
  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  lastReconnectAttempt = 0;
  reconnect();
}

void loop(void)
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 200) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    }
  } else {
    wifi_connect();
  }

  String payload = "{\"INSIDE\":";
  payload += tempCinside;
  payload += ",\"OUTSIDE\":";
  payload += tempCoutside;
  payload += ",\"vdd\":";
  payload += vdd;
  payload += ",\"totalmillis\":";
  payload += (millis() - startMills) ;
  payload += ",\"wifiMills\":";
  payload += wifiMills ;  
  payload += "}";

  if (sendmqttMsg(topic, payload)) {
      if (DEBUG_PRINT) {
        Serial.print("payload pub  ----> ");
        Serial.println(millis() - startMills);
      }
      goingToSleep();
  }  

  if ((millis() - startMills) > 15000) {
    if (DEBUG_PRINT) {
      Serial.println("going to sleep with fail");
    }
    goingToSleep();
  }

}


void goingToSleep()
{
  ESP.deepSleep(300000000);
  delay(250);
}

boolean sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    if (DEBUG_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(payload);
    }

    unsigned int msg_length = payload.length();

    if (DEBUG_PRINT) {
      Serial.print(" length: ");
      Serial.println(msg_length);
    }

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      if (DEBUG_PRINT) {
        Serial.print("Publish ok  --> ");
        Serial.println(millis() - startMills);
      }
      free(p);
      return 1;
    } else {
      if (DEBUG_PRINT) {
        Serial.print("Publish failed --> ");
        Serial.println(millis() - startMills);
      }
      free(p);
      return 0;
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
