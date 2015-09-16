#include <OneWire.h>
#include "DHT.h"
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DallasTemperature.h>
#include <WiFiUdp.h>
#include <Time.h>

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define DEBUG_PRINT 1

// pin
#define pir 13
#define DHTPIN 14     // what pin we're connected to
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
DeviceAddress insideThermometer, outsideThermometer;

// mqtt
char* topic = "esp8266/arduino/s02";
char* subtopic = "esp8266/cmd/light";
char* hellotopic = "HELLO";
IPAddress server(192, 168, 10, 10);

//
unsigned int localPort = 2390;  // local port to listen for UDP packets
IPAddress timeServer(192, 168, 10, 10); // time.nist.gov NTP server
const int timeZone = 9;

//
String clientName;
String payload ;

//
float tempCinside ;
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
long startMills;
unsigned long timemillis;


WiFiClient wifiClient;
PubSubClient client(server, 1883, callback, wifiClient);
WiFiUDP udp;

long lastReconnectAttempt = 0;

void wifi_connect() {
  // WIFI
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
    if (Attempt == 200)
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

  timemillis = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str())) {
        client.publish(hellotopic, "hello again wifi and mqtt from ESP8266 s02");
        client.subscribe(subtopic);

        Serial.println("reconnecting wifi and mqtt");
      } else {
        Serial.println("mqtt publish fail after wifi reconnect");
      }
    } else {
      client.publish(hellotopic, "hello again wifi from ESP8266 s02");
      client.subscribe(subtopic);
    }
  }

}

boolean reconnect()
{
  if (client.connect((char*) clientName.c_str())) {
    Serial.println("connected");
    client.publish(hellotopic, "hello again 1 from ESP8266 s02");
    client.subscribe(subtopic);
  } else {
    Serial.print("failed, rc=");
    Serial.println(client.state());
  }
  timemillis = millis();
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

  if ( receivedpayload == "{\"LIGHT\":1}") {
    relaystatus = 1 ;
  }
  if ( receivedpayload == "{\"LIGHT\":0}") {
    relaystatus = 0 ;
  }

  changelight();

  if (DEBUG_PRINT) {
    Serial.print("");
    Serial.print(" => relaystatus => ");
    Serial.println(relaystatus);
  }
}

void setup()
{
  Serial.begin(38400);
  Serial.println("DHTxx test!");
  Serial.println("ESP.getFlashChipSize() : ");
  Serial.println(ESP.getFlashChipSize());
  delay(20);

  startMills = millis();
  timemillis = millis();

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
  //client.setCallback(callback);

  //
  lastReconnectAttempt = 0;

  String getResetInfo = "hello from ESP8266 s02 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect((char*) clientName.c_str())) {
        client.publish(hellotopic, (char*) getResetInfo.c_str());
        client.subscribe(subtopic);
        Serial.print("Sending payload: ");
        Serial.println(getResetInfo);
      }
    } else {
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      client.subscribe(subtopic);
      Serial.print("Sending payload: ");
      Serial.println(getResetInfo);
    }
  }

  //
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  delay(1000);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    Serial.println("waiting for sync message");
  }

  attachInterrupt(13, motion_detection, RISING);
  attachInterrupt(5, run_lightcmd, CHANGE);

  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  if (!sensors.getAddress(outsideThermometer, 1)) Serial.println("Unable to find address for Device 1");

  // set the resolution to 9 bit
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);

  dht.begin();

  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  sensors.requestTemperatures();
  tempCinside  = sensors.getTempC(outsideThermometer);
  tempCoutside = sensors.getTempC(insideThermometer);

  if ( isnan(tempCinside) || isnan(tempCoutside) ) {
    Serial.println("Failed to read from sensor!");
    return;
  }
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.print("failed, rc=");
      Serial.print(client.state());

      long now = millis();
      if (now - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      client.loop();
    }
  } else {
    wifi_connect();
  }

  changelight();
  runTimerDoLightOff();

  pirValue = digitalRead(pir);

  payload = "{\"Humidity\":";
  payload += h;
  payload += ",\"Temperature\":";
  payload += t;
  payload += ",\"DS18B20\":";
  payload += tempCinside;
  payload += ",\"SENSORTEMP\":";
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


  if ( pirSent == HIGH && pirValue == HIGH )
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
    getdalastempstatus = 0;
    getdht22tempstatus = 0;
    startMills = millis();
  }
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
  if ( relaystatus != oldrelaystatus )
  {
    Serial.print(" => ");
    Serial.println("checking relay status changelight");
    delay(10);
    digitalWrite(RELAYPIN, relaystatus);
    delay(20);
    digitalWrite(RELAYPIN, relaystatus);
    delay(20);
    digitalWrite(RELAYPIN, relaystatus);
    delay(20);
    oldrelaystatus = relaystatus ;
    Serial.print(" => ");
    Serial.println("changing relay status");

    sendlightstatus();
  }
}

void getdht22temp()
{

  h = dht.readHumidity();
  t = dht.readTemperature();
  f = dht.readTemperature(true);

  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
  }

  float hi = dht.computeHeatIndex(f, h);
}

void getdalastemp()
{
  sensors.requestTemperatures();
  tempCinside  = sensors.getTempC(outsideThermometer);
  tempCoutside = sensors.getTempC(insideThermometer);

  if ( isnan(tempCinside) || isnan(tempCoutside) ) {
    Serial.println("Failed to read from sensor!");
  }
}

void sendlightstatus()
{
  String lightpayload = "{\"LIGHT\":";
  lightpayload += relaystatus;
  lightpayload += "}";

  sendmqttMsg(subtopic, lightpayload);
}

void sendmqttMsg(char* topictosend, String payload)
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      client.publish(hellotopic, "hello again 2 from ESP8266 s02");
      client.subscribe(subtopic);
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.print(payload);

    unsigned int msg_length = payload.length();

    Serial.print(" length: ");
    Serial.println(msg_length);

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      Serial.println("Publish ok");
      free(p);
    } else {
      Serial.println("Publish failed");
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
  pirValue = HIGH ;
  pirSent  = HIGH ;
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
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request called");
  sendNTPpacket(timeServer);
  delay(1000);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println(millis() - beginWait);
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address)
{
  Serial.println("Transmit NTP Request");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
  Serial.println("Transmit NTP Sent");
}
//
//
