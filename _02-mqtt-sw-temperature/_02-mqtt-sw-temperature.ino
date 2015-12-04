#include <OneWire.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include <Time.h>

#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define DEBUG_PRINT 0

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

// pin
#define pir 13
#define DHTPIN 14
#define RELAYPIN 4
#define TOPBUTTONPIN 5

// DHT22
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE, 15);

// OTHER
#define REPORT_INTERVAL 9500 // in msec

// DS18B20
#define ONE_WIRE_BUS 12
#define TEMPERATURE_PRECISION 12

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress outsideThermometer;

// mqtt
char* topic = "esp8266/arduino/s02";
char* subtopic = "esp8266/cmd/light";
char* rslttopic = "esp8266/cmd/light/rlst";
char* hellotopic = "HELLO";

char* willTopic = "clients/relay";
char* willMessage = "0";

//
unsigned int localPort = 2390;
const int timeZone = 9;

//
String clientName;
String payload ;

// send reset info
String getResetInfo ;
int ResetInfo = LOW;

//
float tempCoutside ;

float h ;
float t ;
float f ;

//
volatile int pirValue = LOW;
volatile int pirSent  = LOW;

volatile int relaystatus    = LOW;
volatile int oldrelaystatus = LOW;

int getdalastempstatus = 0;
int getdht22tempstatus = 0;

//
unsigned long startMills;
unsigned long timemillis;
unsigned long lastRelayActionmillis;

//
int relayIsReady = HIGH;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
WiFiUDP udp;

long lastReconnectAttempt = 0;

void wifi_connect()
{
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

boolean reconnect()
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
      client.publish(willTopic, "1", true);
      if ( ResetInfo == LOW) {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        ResetInfo = HIGH;
      } else {
        client.publish(hellotopic, "hello again 1 from ESP8266 s02");
      }
      client.subscribe(subtopic);
      if (DEBUG_PRINT) {
        Serial.println("connected");
      }
    } else {
      if (DEBUG_PRINT) {
        Serial.print("failed, rc=");
        Serial.println(client.state());
      }
    }
  }
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

  unsigned long now = millis();

  if ((now - lastRelayActionmillis) >= 30000) {
    if ( receivedpayload == "{\"LIGHT\":1}") {
      relaystatus = 1 ;
    }
    else if ( receivedpayload == "{\"LIGHT\":0}") {
      relaystatus = 0 ;
    }
  }

  if (DEBUG_PRINT) {
    Serial.print("");
    Serial.print(" => relaystatus => ");
    Serial.println(relaystatus);
  }
}

void setup()
{
  if (DEBUG_PRINT) {
    Serial.begin(115200);
  }
  delay(20);
  if (DEBUG_PRINT) {
    Serial.println("Sensor and Relay");
    Serial.println("ESP.getFlashChipSize() : ");
    Serial.println(ESP.getFlashChipSize());
  }
  delay(20);

  startMills = timemillis = lastRelayActionmillis = millis();
  lastRelayActionmillis += 30000;

  pinMode(pir, INPUT);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(TOPBUTTONPIN, INPUT_PULLUP);

  digitalWrite(RELAYPIN, relaystatus);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  //
  lastReconnectAttempt = 0;

  getResetInfo = "hello from ESP8266 s02 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  if (DEBUG_PRINT) {
    Serial.println("Starting UDP");
  }
  udp.begin(localPort);
  if (DEBUG_PRINT) {
    Serial.print("Local port: ");
    Serial.println(udp.localPort());
  }
  delay(1000);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    if (DEBUG_PRINT) {
      Serial.println("waiting for sync message");
    }
  }

  attachInterrupt(13, motion_detection, RISING);
  attachInterrupt(5, run_lightcmd, CHANGE);

  sensors.begin();
  if (!sensors.getAddress(outsideThermometer, 0)) {
    if (DEBUG_PRINT) {
      Serial.println("Unable to find address for Device 0");
    }
  }

  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  dht.begin();

  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    if (DEBUG_PRINT) {
      Serial.println("Failed to read from DHT sensor!");
    }
    return;
  }

  sensors.requestTemperatures();
  tempCoutside  = sensors.getTempC(outsideThermometer);

  if ( isnan(tempCoutside) ) {
    if (DEBUG_PRINT) {
      Serial.println("Failed to read from DS18B20 sensor!");
    }
    return;
  }
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (DEBUG_PRINT) {
        Serial.print("failed, rc=");
        Serial.print(client.state());
      }

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

  if ( relaystatus != oldrelaystatus ) {

    changelight();

    String lightpayload = "{\"LIGHT\":";
    lightpayload += relaystatus;
    lightpayload += ",\"READY\":0";
    lightpayload += "}";

    sendmqttMsg(rslttopic, lightpayload);

  }

  if ((( millis() - lastRelayActionmillis) > 30000 ) && ( relayIsReady == LOW ) && ( relaystatus == oldrelaystatus ))
  {

    String lightpayload = "{\"LIGHT\":";
    lightpayload += relaystatus;
    lightpayload += ",\"READY\":1";
    lightpayload += "}";

    sendmqttMsg(rslttopic, lightpayload);
    relayIsReady = HIGH;
    
  }

  runTimerDoLightOff();

  pirValue = digitalRead(pir);

  payload = "{\"Humidity\":";
  payload += h;
  payload += ",\"Temperature\":";
  payload += t;
  payload += ",\"DS18B20\":";
  payload += tempCoutside;
  payload += ",\"PIRSTATUS\":";
  payload += pirValue;
  payload += ",\"FreeHeap\":";
  payload += ESP.getFreeHeap();
  payload += ",\"RSSI\":";
  payload += WiFi.RSSI();
  payload += ",\"millis\":";
  payload += (millis() - timemillis);
  payload += "}";

  if (( pirSent == HIGH ) && ( pirValue == HIGH ))
  {
    sendmqttMsg(topic, payload);
    pirSent = LOW ;
    startMills = millis();
  }

  if (((millis() - startMills) > REPORT_INTERVAL ) && ( getdalastempstatus == 0))
  {
    getdalastemp();
    getdalastempstatus = 1;
  }

  if (((millis() - startMills) > REPORT_INTERVAL ) && ( getdht22tempstatus == 0))
  {
    getdht22temp();
    getdht22tempstatus = 1;
  }

  if ((millis() - startMills) > REPORT_INTERVAL )
  {
    sendmqttMsg(topic, payload);
    getdalastempstatus = getdht22tempstatus = 0;
    startMills = millis();
  }

  client.loop();

}

void runTimerDoLightOff()
{
  if (( relaystatus == 1 ) && ( hour() == 6 ) && ( minute() == 00 ) && ( second() < 5 ))
  {
    relaystatus = 0;
  }
}

void changelight()
{

  if (DEBUG_PRINT) {
    Serial.print(" => ");
    Serial.println("checking relay status changelight");
  }

  digitalWrite(RELAYPIN, relaystatus);
  delay(50);

  oldrelaystatus = relaystatus ;
  relayIsReady = LOW;

  if (DEBUG_PRINT) {
    Serial.print(" => ");
    Serial.println("changing relay status");
  }

  lastRelayActionmillis = millis();
}

void getdht22temp()
{

  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    if (DEBUG_PRINT) {
      Serial.println("Failed to read from DHT sensor!");
    }
  }

  float hi = dht.computeHeatIndex(f, h);
}

void getdalastemp()
{
  sensors.requestTemperatures();
  tempCoutside  = sensors.getTempC(outsideThermometer);

  if ( isnan(tempCoutside)  ) {
    if (DEBUG_PRINT) {
      Serial.println("Failed to read from sensor!");
    }
  }
}

void sendmqttMsg(char* topictosend, String payload)
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

void run_lightcmd()
{
  int topbuttonstatus =  ! digitalRead(TOPBUTTONPIN);
  relaystatus = topbuttonstatus ;
}

void motion_detection()
{
  pirValue = pirSent = HIGH ;
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

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ;
  if (DEBUG_PRINT) {
    Serial.println("Transmit NTP Request called");
  }
  sendNTPpacket(time_server);
  delay(1000);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      if (DEBUG_PRINT) {
        Serial.println("Receive NTP Response");
      }
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  if (DEBUG_PRINT) {
    Serial.println(millis() - beginWait);
    Serial.println("No NTP Response :-(");
  }
  return 0;
}

void sendNTPpacket(IPAddress & address)
{
  if (DEBUG_PRINT) {
    Serial.println("Transmit NTP Request");
  }
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  if (DEBUG_PRINT) {
    Serial.println("Transmit NTP Sent");
  }
}
//
