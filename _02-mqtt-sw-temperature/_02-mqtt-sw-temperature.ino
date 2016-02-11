// 80M CPU / 4M / 1M SPIFFS / esp-swtemp
// with #define DHT_DEBUG_TIMING on / PietteTech_DHT-8266
#include <TimeLib.h>
//#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
//#include <pgmspace.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DallasTemperature.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
//#include "DHT.h"
// for esp8266
// https://github.com/chaeplin/PietteTech_DHT-8266
#include "PietteTech_DHT.h"

// radio
#define DEVICE_ID 1
#define CHANNEL 100 //MAX 127

extern "C" {
#include "user_interface.h"
}

#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define DEBUG_PRINT 0
//#define DHT_DEBUG_TIMING 

// ****************
time_t getNtpTime();
void callback(char* intopic, byte* inpayload, unsigned int length);
String macToStr(const uint8_t* mac);
void run_lightcmd();
void changelight();
void sendmqttMsg(char* topictosend, String payload);
void runTimerDoLightOff();
//void getdalastemp();
//void getdht22temp();
void sendNTPpacket(IPAddress & address);
void printEdgeTiming(class PietteTech_DHT *_d);

static unsigned long  numberOfSecondsSinceEpochUTC(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s);
long DateToMjd (uint16_t y, uint8_t m, uint8_t d);
void sendUdpmsg(String msgtosend);


// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

//
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

// pin
#define pir 16
//#define DHTPIN 2
#define RELAYPIN 4
#define TOPBUTTONPIN 5

// system defines
#define DHTTYPE  DHT22              // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   2              // Digital pin for communications

// OTHER
//#define REPORT_INTERVAL 9500 // in msec
#define REPORT_INTERVAL 5000 // in msec

#define BETWEEN_RELAY_ACTIVE 5000

// DS18B20
//#define ONE_WIRE_BUS 12
#define ONE_WIRE_BUS 0
#define TEMPERATURE_PRECISION 9

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress outsideThermometer;

// radio
RF24 radio(3, 15);

// Topology
const uint64_t pipes[3] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL, 0xFFFFFFFFCCLL };

const uint32_t ampereunit[]  = { 0, 1000000, 1, 1000};

typedef struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t data1;
  int16_t data2;
  uint8_t devid;
} data;

data sensor_data;

// mqtt
char* topic = "esp8266/arduino/s02";
char* subtopic = "esp8266/cmd/light";
char* rslttopic = "esp8266/cmd/light/rlst";
char* hellotopic = "HELLO";
char* radiotopic = "radio/test";
char* radiofault = "radio/test/fault";

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

//
int pirValue ;
int pirSent  ;
int oldpirValue ;

//
volatile int relaystatus    = LOW;
int oldrelaystatus = LOW;

//
unsigned long startMills;
unsigned long timemillis;
unsigned long lastRelayActionmillis;

//
uint32_t timestamp;
int millisnow;

//
int relayIsReady = HIGH;

//////////////////
int acquireresult;
bool bDalasstarted;
float t, h;

//declaration
void dht_wrapper(); // must be declared before the lib initialization

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
bool bDHTstarted;       // flag to indicate we started acquisition

// This wrapper is in charge of calling
// must be defined like this for the lib work
void dht_wrapper() {
  DHT.isrCallback();
}

/////////////
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

  //wifi_set_phy_mode(PHY_MODE_11N);
  //wifi_set_channel(channel);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (DEBUG_PRINT) {
      Serial.print(".");
    }
    if (Attempt == 300)
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

  if ( now < 15000 ) {
    if ( receivedpayload == "{\"LIGHT\":1}") {
      relaystatus = HIGH ;
    }
    if ( receivedpayload == "{\"LIGHT\":0}") {
      relaystatus = LOW ;
    }
    relayIsReady = LOW;
  } else if ((now - lastRelayActionmillis) >= BETWEEN_RELAY_ACTIVE ) {
    if ( receivedpayload == "{\"LIGHT\":1}") {
      relaystatus = HIGH ;
    }
    if ( receivedpayload == "{\"LIGHT\":0}") {
      relaystatus = LOW ;
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
  yield();
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
  lastRelayActionmillis += BETWEEN_RELAY_ACTIVE;

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
  millisnow = 0;

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
      setSyncProvider(getNtpTime);
    }
  }

  //
  //attachInterrupt(16, motion_detection, RISING);
  attachInterrupt(5, run_lightcmd, CHANGE);

  pirSent = LOW ;
  pirValue = oldpirValue = digitalRead(pir);

  sensors.begin();
  if (!sensors.getAddress(outsideThermometer, 0)) {
    if (DEBUG_PRINT) {
      Serial.println("Unable to find address for Device 0");
    }
  }

  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  tempCoutside = sensors.getTempC(outsideThermometer);

  if ( isnan(tempCoutside) ) {
    if (DEBUG_PRINT) {
      Serial.println("Failed to read from DS18B20 sensor!");
    }
    //return;
  }

  // radio
  //yield();

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_HIGH);
  //  radio.setDataRate(RF24_250KBPS);
  radio.setDataRate(RF24_1MBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  //radio.setCRCLength(RF24_CRC_8);
  //radio.setPayloadSize(11);
  radio.enableDynamicPayloads();
  radio.openReadingPipe(1, pipes[0]);
  radio.openReadingPipe(2, pipes[2]);
  //radio.openWritingPipe(pipes[1]);
  radio.startListening();


  //OTA
  // Port defaults to 8266
  //ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("esp-swtemp");

  // No authentication by default
  ArduinoOTA.setPassword(otapassword);

  ArduinoOTA.onStart([]() {
    //Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    //Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    //ESP.restart();
    if (error == OTA_AUTH_ERROR) abort();
    else if (error == OTA_BEGIN_ERROR) abort();
    else if (error == OTA_CONNECT_ERROR) abort();
    else if (error == OTA_RECEIVE_ERROR) abort();
    else if (error == OTA_END_ERROR) abort();

  });

  ArduinoOTA.begin();

  acquireresult = DHT.acquireAndWait(0);
  if ( acquireresult == 0 ) {
    t = DHT.getCelsius();
    h = DHT.getHumidity();
  } else {
    t = h = 0;
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
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 500) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      if (bDHTstarted) {
        if (!DHT.acquiring()) {
          acquireresult = DHT.getStatus();
#if defined(DHT_DEBUG_TIMING)    
          printEdgeTiming(&DHT);
#endif
          if ( acquireresult == 0 ) {
            t = DHT.getCelsius();
            h = DHT.getHumidity();
          }
          bDHTstarted = false;
        }
      }

      if (bDalasstarted) {
        if (millis() > (startMills + (750 / (1 << (12 - TEMPERATURE_PRECISION))))) {
          tempCoutside  = sensors.getTempC(outsideThermometer);
          //tempCoutside  = sensors.getTempCByIndex(0);
          bDalasstarted = false;
        }
      }

      if ( relaystatus != oldrelaystatus ) {

        if (DEBUG_PRINT) {
          Serial.print("call changelight  => relaystatus => ");
          Serial.println(relaystatus);
        }

        changelight();

        if (DEBUG_PRINT) {
          Serial.print("after changelight  => relaystatus => ");
          Serial.println(relaystatus);
        }

        String lightpayload = "{\"LIGHT\":";
        lightpayload += relaystatus;
        lightpayload += ",\"READY\":0";
        lightpayload += "}";

        sendmqttMsg(rslttopic, lightpayload);

      }

      if ((relayIsReady == LOW ) &&  ( millis() < 15000 ) && ( relaystatus == oldrelaystatus ))
      {

        if (DEBUG_PRINT) {
          Serial.print("after BETWEEN_RELAY_ACTIVE => relaystatus => ");
          Serial.println(relaystatus);
        }

        String lightpayload = "{\"LIGHT\":";
        lightpayload += relaystatus;
        lightpayload += ",\"READY\":1";
        lightpayload += "}";

        sendmqttMsg(rslttopic, lightpayload);
        relayIsReady = HIGH;

      } else if ((relayIsReady == LOW ) &&  (( millis() - lastRelayActionmillis) > BETWEEN_RELAY_ACTIVE ) && ( relaystatus == oldrelaystatus ))
      {

        if (DEBUG_PRINT) {
          Serial.print("after BETWEEN_RELAY_ACTIVE => relaystatus => ");
          Serial.println(relaystatus);
        }

        String lightpayload = "{\"LIGHT\":";
        lightpayload += relaystatus;
        lightpayload += ",\"READY\":1";
        lightpayload += "}";

        sendmqttMsg(rslttopic, lightpayload);
        relayIsReady = HIGH;
      }

      runTimerDoLightOff();

      pirValue = digitalRead(pir);
      if ( oldpirValue != pirValue ) {
        pirSent = HIGH;
        oldpirValue = pirValue;
      }

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
      payload += ",\"acquireresult\":";
      payload += acquireresult;
      payload += ",\"RSSI\":";
      payload += WiFi.RSSI();
      payload += ",\"millis\":";
      payload += (millis() - timemillis);
      payload += "}";

      if ( pirSent == HIGH )
      {
        sendmqttMsg(topic, payload);
        pirSent = LOW;
        //startMills = millis();
      }

      if ((millis() - startMills) > REPORT_INTERVAL )
      {
        sendmqttMsg(topic, payload);
        //getdalastempstatus = getdht22tempstatus = 0;
        startMills = millis();
        sensors.setWaitForConversion(false);
        sensors.requestTemperatures();
        sensors.setWaitForConversion(true);
        bDalasstarted = true;

        DHT.acquire();
        bDHTstarted = true;
      }

      // radio
      if (radio.available()) {
        // from attiny 85 data size is 11
        // sensor_data data size = 12
        uint8_t len = radio.getDynamicPayloadSize();
        // avr 8bit, esp 32bit. esp use 4 byte step.
        if ( (len + 1 ) != sizeof(sensor_data) ) {
          if (DEBUG_PRINT) {
            Serial.print(" ****** radio ======> len : sizeof(sensor_data) : return : ");
            Serial.print(len);
            Serial.print(" : ");
            Serial.println(sizeof(sensor_data));
          }
          radio.read(0, 0);
          return;
        }
        radio.read(&sensor_data, sizeof(sensor_data));

        if (DEBUG_PRINT) {
          Serial.print(" ****** radio ======> size : ");
          Serial.print(sizeof(sensor_data));
          Serial.print(" _salt : ");
          Serial.print(sensor_data._salt);
          Serial.print(" volt : ");
          Serial.print(sensor_data.volt);
          Serial.print(" data1 : ");
          Serial.print(sensor_data.data1);
          Serial.print(" data2 : ");
          Serial.print(sensor_data.data2);
          Serial.print(" dev_id :");
          Serial.println(sensor_data.devid);
        }

        if ( sensor_data.devid != 15 ) {

          String radiopayload = "{\"_salt\":";
          radiopayload += sensor_data._salt;
          radiopayload += ",\"volt\":";
          radiopayload += sensor_data.volt;

          radiopayload += ",\"data1\":";
          if ( sensor_data.data1 == 0 ) {
            radiopayload += 0;
          } else {
            radiopayload += ((float)sensor_data.data1 / 10);
          }

          radiopayload += ",\"data2\":";
          if ( sensor_data.data2 == 0 ) {
            radiopayload += 0;
          } else {
            radiopayload += ((float)sensor_data.data2 / 10);
          }

          radiopayload += ",\"devid\":";
          radiopayload += sensor_data.devid;
          radiopayload += "}";

          if ( (sensor_data.devid > 0) && (sensor_data.devid < 255) )
          {
            String newRadiotopic = radiotopic;
            newRadiotopic += "/";
            newRadiotopic += sensor_data.devid;

            unsigned int newRadiotopic_length = newRadiotopic.length();
            char newRadiotopictosend[newRadiotopic_length] ;
            newRadiotopic.toCharArray(newRadiotopictosend, newRadiotopic_length + 1);
            sendmqttMsg(newRadiotopictosend, radiopayload);
          } else {
            sendmqttMsg(radiofault, radiopayload);
          }
        } else {
          if (timeStatus() != timeNotSet) {
            timestamp = numberOfSecondsSinceEpochUTC(year(), month(), day(), hour(), minute(), second());
            millisnow = millisecond();
            //}

            if ( sensor_data.data1 < 0 ) {
              sensor_data.data1 = 0;
            }

            String udppayload = "current,test=current,measureno=";
            udppayload += sensor_data._salt;
            udppayload += " devid=";
            udppayload += sensor_data.devid;
            udppayload += "i,volt=";
            udppayload += sensor_data.volt;
            udppayload += "i,ampere=";

            uint32_t ampere_temp;
            ampere_temp = sensor_data.data1 * ampereunit[sensor_data.data2];
            udppayload += ampere_temp;
            udppayload += " ";
            udppayload += timestamp;

            char buf[3];
            sprintf(buf, "%03d", millisnow);
            udppayload += buf;
            udppayload += "000000";
            sendUdpmsg(udppayload);
          }
        }
      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}

void runTimerDoLightOff()
{
  if (( relaystatus == HIGH ) && ( hour() == 6 ) && ( minute() == 00 ) && ( second() < 5 ))
  {
    if (DEBUG_PRINT) {
      Serial.print(" => ");
      Serial.print("checking relay status runTimerDoLightOff --> ");
      Serial.println(relaystatus);
    }
    relaystatus = LOW;
  }
}

void changelight()
{
  if (DEBUG_PRINT) {
    Serial.print(" => ");
    Serial.print("checking relay status changelight --> ");
    Serial.println(relaystatus);
  }

  relayIsReady = LOW;
  digitalWrite(RELAYPIN, relaystatus);
  //delay(50);

  if (DEBUG_PRINT) {
    Serial.print(" => ");
    Serial.print("changing relay status --> ");
    Serial.println(relaystatus);
  }

  lastRelayActionmillis = millis();
  oldrelaystatus = relaystatus ;
  //relayIsReady = LOW;
}

void sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    if (DEBUG_PRINT) {
      Serial.print("Sending payload: ");
      Serial.print(topictosend);
      Serial.print(" - ");
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
  //int topbuttonstatus =  ! digitalRead(TOPBUTTONPIN);
  if ( relayIsReady == HIGH  ) {
    //relaystatus = topbuttonstatus ;
    relaystatus = !relaystatus;
  }
  if (DEBUG_PRINT && ( relayIsReady == HIGH  )) {
    //Serial.print("run_lightcmd  => topbuttonstatus => ");
    //Serial.print(topbuttonstatus);
    Serial.print(" => relaystatus => ");
    Serial.println(relaystatus);
  }
}

// pin 16 can't be used for Interrupts
/*
  void motion_detection()
  {
  if (DEBUG_PRINT) {
    Serial.println("motion_detection called");
  }
  //pirValue =
  pirValue = digitalRead(pir);
  pirSent = HIGH ;
  }
*/

//
void sendUdpmsg(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
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

void printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
#if defined(DHT_DEBUG_TIMING)  
  volatile uint8_t *_e = &_d->_edges[0];
#endif
  String udppayload = "edges2,device=esp-12-N1,debug=on ";
  for (n = 0; n < 41; n++) {
    char buf[2];
    if ( n < 40 ) {
      udppayload += "e";
      sprintf(buf, "%02d", n);
      udppayload += buf;
      udppayload += "=";
#if defined(DHT_DEBUG_TIMING)       
      udppayload += *_e++;
#endif      
      udppayload += "i,";
    } else {
      udppayload += "e";
      sprintf(buf, "%02d", n);
      udppayload += buf;
      udppayload += "=";
#if defined(DHT_DEBUG_TIMING)        
      udppayload += *_e++;
#endif      
      udppayload += "i";
    }
  }
  sendUdpmsg(udppayload);
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
  delay(3000);
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


long DateToMjd (uint16_t y, uint8_t m, uint8_t d)
{
  return
    367 * y
    - 7 * (y + (m + 9) / 12) / 4
    - 3 * ((y + (m - 9) / 7) / 100 + 1) / 4
    + 275 * m / 9
    + d
    + 1721028
    - 2400000;
}

static unsigned long  numberOfSecondsSinceEpochUTC(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s)
{
  long Days;

  Days = DateToMjd(y, m, d) - DateToMjd(1970, 1, 1);
  return (uint16_t)Days * 86400 + h * 3600L + mm * 60L + s - (timeZone * SECS_PER_HOUR);
}


//
