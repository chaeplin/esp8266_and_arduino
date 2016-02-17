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

// radio
#define DEVICE_ID 1
#define CHANNEL 100 //MAX 127

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

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

#define INFO_PRINT 0
#define DEBUG_PRINT 0

// ****************
time_t getNtpTime();
void callback(char* intopic, byte* inpayload, unsigned int length);
String macToStr(const uint8_t* mac);
void run_lightcmd();
void changelight();
void sendmqttMsg(char* topictosend, String payload);
void runTimerDoLightOff();
void sendNTPpacket(IPAddress & address);
void sendUdpSyslog(String msgtosend);

static unsigned long  numberOfSecondsSinceEpochUTC(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s);
long DateToMjd (uint16_t y, uint8_t m, uint8_t d);
void sendUdpmsg(String msgtosend);
void check_radio();

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
#define RELAYPIN 4
#define TOPBUTTONPIN 5

// OTHER
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
#define radiointPin 2

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

bool bDalasstarted;

/////////////
WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
WiFiUDP udp;

long lastReconnectAttempt = 0;

void wifi_connect()
{
  // WIFI
  if (INFO_PRINT) {
    sendUdpSyslog("Connecting to ");
    sendUdpSyslog(ssid);
  }

  wifi_set_phy_mode(PHY_MODE_11N);
  //wifi_set_channel(channel);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (INFO_PRINT) {
      sendUdpSyslog(".");
    }
    if (Attempt == 300)
    {
      if (INFO_PRINT) {
        sendUdpSyslog("Could not connect to WIFI");
      }
      ESP.restart();
    }
  }

  if (INFO_PRINT) {
    sendUdpSyslog("WiFi connected");
    sendUdpSyslog("IP address: ");
    sendUdpSyslog(WiFi.localIP().toString().c_str());
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
      if (INFO_PRINT) {
        sendUdpSyslog("connected");
      }
    } else {
      if (INFO_PRINT) {
        sendUdpSyslog("failed, rc=");
        sendUdpSyslog(String(client.state()));
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

  if (INFO_PRINT) {
    sendUdpSyslog(intopic);
    sendUdpSyslog(" => ");
    sendUdpSyslog(receivedpayload);
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

  if (INFO_PRINT) {
    sendUdpSyslog(" => relaystatus => ");
    sendUdpSyslog(String(relaystatus));
  }
}

void setup()
{
  system_update_cpu_freq(SYS_CPU_80MHz);
  // wifi_status_led_uninstall();
  Serial.end();

  delay(20);
  if (INFO_PRINT) {
    sendUdpSyslog("Sensor and Relay");
    sendUdpSyslog("ESP.getFlashChipSize() : ");
    sendUdpSyslog(String(ESP.getFlashChipSize()));
  }
  delay(20);

  startMills = timemillis = lastRelayActionmillis = millis();
  lastRelayActionmillis += BETWEEN_RELAY_ACTIVE;

  pinMode(pir, INPUT);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(TOPBUTTONPIN, INPUT_PULLUP);
  pinMode(radiointPin, INPUT_PULLUP);
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

  if (INFO_PRINT) {
    sendUdpSyslog("Starting UDP");
  }
  udp.begin(localPort);
  if (INFO_PRINT) {
    sendUdpSyslog("Local port: ");
    sendUdpSyslog(String(udp.localPort()));
  }
  delay(1000);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    if (INFO_PRINT) {
      sendUdpSyslog("waiting for sync message");
      setSyncProvider(getNtpTime);
    }
  }

  attachInterrupt(5, run_lightcmd, CHANGE);
  attachInterrupt(2, check_radio, FALLING);

  pirSent = LOW ;
  pirValue = oldpirValue = digitalRead(pir);

  sensors.begin();
  if (!sensors.getAddress(outsideThermometer, 0)) {
    if (INFO_PRINT) {
      sendUdpSyslog("Unable to find address for Device 0");
    }
  }

  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  tempCoutside = sensors.getTempC(outsideThermometer);

  if ( isnan(tempCoutside) ) {
    if (INFO_PRINT) {
      sendUdpSyslog("Failed to read from DS18B20 sensor!");
    }
    //return;
  }

  // radio
  //yield();

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_MAX);
  //  radio.setDataRate(RF24_250KBPS);
  radio.setDataRate(RF24_1MBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  //radio.setCRCLength(RF24_CRC_8);
  //radio.setPayloadSize(11);
  radio.enableDynamicPayloads();
  radio.maskIRQ(1, 1, 0);
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
    //sendUdpSyslog("Start");
  });
  ArduinoOTA.onEnd([]() {
    //sendUdpSyslog("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //sendUdpSyslog("Progress: %u%%\r", (progress / (total / 100)));
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

  if (DEBUG_PRINT) {
    sendUdpSyslog("------------------> unit started");
    sendUdpSyslog(String(digitalRead(2)));
  }
}

/*
  void check_radio() {
  bool tx, fail, rx;
  radio.whatHappened(tx, fail, rx);                   // What happened?

  if (DEBUG_PRINT) {
    sendUdpSyslog(" radio called  : ");
    sendUdpSyslog(String(rx));
  }
  }
*/


void check_radio() {
  bool tx, fail, rx;
  radio.whatHappened(tx, fail, rx);  // What happened?

  // If data is available, handle it accordingly
  if ( rx ) {

    if (radio.getDynamicPayloadSize() < 1) {
      // Corrupt payload has been flushed
      return;
    }
    // from attiny 85 data size is 11
    // sensor_data data size = 12
    uint8_t len = radio.getDynamicPayloadSize();

    if (DEBUG_PRINT) {
      sendUdpSyslog(" ****** getDynamicPayloadSize ======>  : ");
      sendUdpSyslog(String(len));
    }

    // avr 8bit, esp 32bit. esp use 4 byte step.
    if ( (len + 1 ) != sizeof(sensor_data) ) {
      if (INFO_PRINT) {
        sendUdpSyslog(" ****** radio ======> len : ");
        sendUdpSyslog(String(len));
        sendUdpSyslog(" : ");
        sendUdpSyslog(String(sizeof(sensor_data)));
      }
      radio.read(0, 0);
      return;
    }
    radio.read(&sensor_data, sizeof(sensor_data));


    if (INFO_PRINT) {
      sendUdpSyslog(" ****** radio ======> size : ");
      sendUdpSyslog(String(sizeof(sensor_data)));
      sendUdpSyslog(" _salt : ");
      sendUdpSyslog(String(sensor_data._salt));
      sendUdpSyslog(" volt : ");
      sendUdpSyslog(String(sensor_data.volt));
      sendUdpSyslog(" data1 : ");
      sendUdpSyslog(String(sensor_data.data1));
      sendUdpSyslog(" data2 : ");
      sendUdpSyslog(String(sensor_data.data2));
      sendUdpSyslog(" dev_id :");
      sendUdpSyslog(String(sensor_data.devid));
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
}


void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (INFO_PRINT) {
        sendUdpSyslog("failed, rc=");
        sendUdpSyslog(String(client.state()));
      }
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 500) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      if (bDalasstarted) {
        if (millis() > (startMills + (750 / (1 << (12 - TEMPERATURE_PRECISION))))) {
          tempCoutside  = sensors.getTempC(outsideThermometer);
          //tempCoutside  = sensors.getTempCByIndex(0);
          bDalasstarted = false;
        }
      }

      if ( relaystatus != oldrelaystatus ) {

        if (INFO_PRINT) {
          sendUdpSyslog("call changelight  => relaystatus => ");
          sendUdpSyslog(String(relaystatus));
        }

        changelight();

        if (INFO_PRINT) {
          sendUdpSyslog("after changelight  => relaystatus => ");
          sendUdpSyslog(String(relaystatus));
        }

        String lightpayload = "{\"LIGHT\":";
        lightpayload += relaystatus;
        lightpayload += ",\"READY\":0";
        lightpayload += "}";

        sendmqttMsg(rslttopic, lightpayload);

      }

      if ((relayIsReady == LOW ) &&  ( millis() < 15000 ) && ( relaystatus == oldrelaystatus ))
      {

        if (INFO_PRINT) {
          sendUdpSyslog("after BETWEEN_RELAY_ACTIVE => relaystatus => ");
          sendUdpSyslog(String(relaystatus));
        }

        String lightpayload = "{\"LIGHT\":";
        lightpayload += relaystatus;
        lightpayload += ",\"READY\":1";
        lightpayload += "}";

        sendmqttMsg(rslttopic, lightpayload);
        relayIsReady = HIGH;

      } else if ((relayIsReady == LOW ) &&  (( millis() - lastRelayActionmillis) > BETWEEN_RELAY_ACTIVE ) && ( relaystatus == oldrelaystatus ))
      {

        if (INFO_PRINT) {
          sendUdpSyslog("after BETWEEN_RELAY_ACTIVE => relaystatus => ");
          sendUdpSyslog(String(relaystatus));
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

      payload = "{\"DS18B20\":";
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

      if ( pirSent == HIGH )
      {
        sendmqttMsg(topic, payload);
        pirSent = LOW;
        //startMills = millis();
      }

      /*
        // radio
        if (radio.available()) {
        // from attiny 85 data size is 11
        // sensor_data data size = 12
        uint8_t len = radio.getDynamicPayloadSize();

        if (DEBUG_PRINT) {
          sendUdpSyslog(" ****** getDynamicPayloadSize ======>  : ");
          sendUdpSyslog(String(digitalRead(2)));
          sendUdpSyslog(String(len));
        }

        // avr 8bit, esp 32bit. esp use 4 byte step.
        if ( (len + 1 ) != sizeof(sensor_data) ) {
          if (INFO_PRINT) {
            sendUdpSyslog(" ****** radio ======> len : ");
            sendUdpSyslog(String(len));
            sendUdpSyslog(" : ");
            sendUdpSyslog(String(sizeof(sensor_data)));
          }
          radio.read(0, 0);
          return;
        }
        radio.read(&sensor_data, sizeof(sensor_data));


        if (INFO_PRINT) {
          sendUdpSyslog(" ****** radio ======> size : ");
          sendUdpSyslog(String(sizeof(sensor_data)));
          sendUdpSyslog(" _salt : ");
          sendUdpSyslog(String(sensor_data._salt));
          sendUdpSyslog(" volt : ");
          sendUdpSyslog(String(sensor_data.volt));
          sendUdpSyslog(" data1 : ");
          sendUdpSyslog(String(sensor_data.data1));
          sendUdpSyslog(" data2 : ");
          sendUdpSyslog(String(sensor_data.data2));
          sendUdpSyslog(" dev_id :");
          sendUdpSyslog(String(sensor_data.devid));
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
      */

      if ((millis() - startMills) > REPORT_INTERVAL )
      {
        sendmqttMsg(topic, payload);
        //getdalastempstatus = getdht22tempstatus = 0;
        startMills = millis();
        sensors.setWaitForConversion(false);
        sensors.requestTemperatures();
        sensors.setWaitForConversion(true);
        bDalasstarted = true;
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
    if (INFO_PRINT) {
      sendUdpSyslog(" => ");
      sendUdpSyslog("checking relay status runTimerDoLightOff --> ");
      sendUdpSyslog(String(relaystatus));
    }
    relaystatus = LOW;
  }
}

void changelight()
{
  if (INFO_PRINT) {
    sendUdpSyslog(" => ");
    sendUdpSyslog("checking relay status changelight --> ");
    sendUdpSyslog(String(relaystatus));
  }

  relayIsReady = LOW;
  digitalWrite(RELAYPIN, relaystatus);
  //delay(50);

  if (INFO_PRINT) {
    sendUdpSyslog(" => ");
    sendUdpSyslog("changing relay status --> ");
    sendUdpSyslog(String(relaystatus));
  }

  lastRelayActionmillis = millis();
  oldrelaystatus = relaystatus ;
  //relayIsReady = LOW;
}

void sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {

    if (INFO_PRINT) {
      sendUdpSyslog("Sending payload: ");
      sendUdpSyslog(topictosend);
      sendUdpSyslog(" - ");
      sendUdpSyslog(payload);
    }

    unsigned int msg_length = payload.length();

    if (INFO_PRINT) {
      sendUdpSyslog(" length: ");
      sendUdpSyslog(String(msg_length));
    }

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length, 1)) {
      if (INFO_PRINT) {
        sendUdpSyslog("Publish ok");
      }
      free(p);
    } else {
      if (INFO_PRINT) {
        sendUdpSyslog("Publish failed");
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
  if (INFO_PRINT && ( relayIsReady == HIGH  )) {
    //sendUdpSyslog("run_lightcmd  => topbuttonstatus => ");
    //sendUdpSyslog(topbuttonstatus);
    sendUdpSyslog(" => relaystatus => ");
    sendUdpSyslog(String(relaystatus));
  }
}

// pin 16 can't be used for Interrupts

void sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-sw-temp: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

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

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ;
  if (INFO_PRINT) {
    sendUdpSyslog("Transmit NTP Request called");
  }
  sendNTPpacket(time_server);
  delay(3000);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      if (INFO_PRINT) {
        sendUdpSyslog("Receive NTP Response");
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
  if (INFO_PRINT) {
    sendUdpSyslog(String(millis() - beginWait));
    sendUdpSyslog("No NTP Response :-(");
  }
  return 0;
}

void sendNTPpacket(IPAddress & address)
{
  if (INFO_PRINT) {
    sendUdpSyslog("Transmit NTP Request");
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
  if (INFO_PRINT) {
    sendUdpSyslog("Transmit NTP Sent");
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
