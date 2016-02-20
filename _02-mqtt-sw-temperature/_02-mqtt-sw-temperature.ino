// 160M CPU / 4M / 1M SPIFFS / esp-swtemp
#include <TimeLib.h>
#include "nRF24L01.h"
#include "RF24.h"
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

#define INFO_PRINT 1
#define DEBUG_PRINT 1

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
void radio_publish();

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
unsigned int localPort = 12390;
const int timeZone = 9;

//
String clientName;
String payload;

String syslogPayload;

// send reset info
String getResetInfo;
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
volatile bool radioiswait;

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

void wifi_connect() {
  //wifi_set_phy_mode(PHY_MODE_11N);
  //wifi_set_channel(channel);

  //WiFiClient::setLocalPortStart(micros());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      ESP.restart();
    }
  }
}

boolean reconnect() {
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
        sendUdpSyslog("---> mqttconnected");
      }
    } else {
      if (DEBUG_PRINT) {
        syslogPayload = "failed, rc=";
        syslogPayload += client.state();
        sendUdpSyslog(syslogPayload);
      }
    }
  }
  return client.connected();
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length) {
  String receivedtopic = intopic;
  String receivedpayload ;

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }

  if (INFO_PRINT) {
    syslogPayload = intopic;
    syslogPayload += " ====> ";
    syslogPayload += receivedpayload;
    sendUdpSyslog(syslogPayload);
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
    syslogPayload = " => relaystatus => ";
    syslogPayload += relaystatus;
    sendUdpSyslog(syslogPayload);
  }
}

void setup() {
  system_update_cpu_freq(SYS_CPU_160MHz);
  // wifi_status_led_uninstall();

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

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    if (INFO_PRINT) {
      sendUdpSyslog("waiting for sync message");
      setSyncProvider(getNtpTime);
    }
  }

  attachInterrupt(5, run_lightcmd, CHANGE);
  attachInterrupt(2, check_radio, FALLING);
  // ONLOW HAS ERROR
  //attachInterrupt(2, check_radio, ONLOW);

  radioiswait = false;

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
  sensors.setWaitForConversion(false);

  if ( isnan(tempCoutside) ) {
    if (INFO_PRINT) {
      sendUdpSyslog("Failed to read from DS18B20 sensor!");
    }
    //return;
  }

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(15, 15);
  radio.enableDynamicPayloads();
  radio.maskIRQ(1, 1, 0);
  radio.openReadingPipe(1, pipes[0]);
  radio.openReadingPipe(2, pipes[2]);
  radio.startListening();

  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-swtemp");
  ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]() {
    sendUdpSyslog("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]() {
    sendUdpSyslog("ArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    syslogPayload = "Progress: ";
    syslogPayload += (progress / (total / 100));
    sendUdpSyslog(syslogPayload);
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
    syslogPayload = "------------------> unit started : pin 2 status : ";
    syslogPayload += digitalRead(2);
    sendUdpSyslog(syslogPayload);
  }
}

void ICACHE_RAM_ATTR check_radio() {
  bool tx, fail, rx;
  radio.whatHappened(tx, fail, rx);  // What happened?

  // If data is available, handle it accordingly
  //if ( rx ) {

  if (radio.getDynamicPayloadSize() < 1) {
    // Corrupt payload has been flushed
    return;
  }
  // from attiny 85 data size is 11
  // sensor_data data size = 12
  uint8_t len = radio.getDynamicPayloadSize();
  // avr 8bit, esp 32bit. esp use 4 byte step.
  if ( (len + 1 ) != sizeof(sensor_data) ) {
    radio.read(0, 0);
    return;
  }
  radio.read(&sensor_data, sizeof(sensor_data));
  radioiswait = true;
  //}
}

void ICACHE_RAM_ATTR radio_publish() {
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


void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (DEBUG_PRINT) {
        syslogPayload = "failed, rc= ";
        syslogPayload += client.state();
        sendUdpSyslog(syslogPayload);
      }
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 100) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      
      if (radioiswait) {
        radio_publish();
        radioiswait = false;
      }
      

      if (bDalasstarted) {
        if (millis() > (startMills + (750 / (1 << (12 - TEMPERATURE_PRECISION))))) {
          unsigned long getTempCstart =  micros();
          tempCoutside  = sensors.getTempC(outsideThermometer);
          unsigned long getTempCstop =  micros();
          bDalasstarted = false;

          /*
          if (DEBUG_PRINT) {
            syslogPayload = "getTempC delay : ";
            syslogPayload += (getTempCstop - getTempCstart) ;
            sendUdpSyslog(syslogPayload);
          }
          */
        }
      }

      if ( relaystatus != oldrelaystatus ) {

        if (INFO_PRINT) {
          syslogPayload = "call change light => relaystatus => ";
          syslogPayload += relaystatus;
          sendUdpSyslog(syslogPayload);
        }

        changelight();

        if (INFO_PRINT) {
          syslogPayload = "after change light => relaystatus => ";
          syslogPayload += relaystatus;
          sendUdpSyslog(syslogPayload);
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
          syslogPayload = "after BETWEEN_RELAY_ACTIVE => relaystatus => ";
          syslogPayload += relaystatus;
          sendUdpSyslog(syslogPayload);
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
          syslogPayload = "after BETWEEN_RELAY_ACTIVE => relaystatus => ";
          syslogPayload += relaystatus;
          sendUdpSyslog(syslogPayload);
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

      if ((millis() - startMills) > REPORT_INTERVAL )
      {
        sendmqttMsg(topic, payload);

        unsigned long requestTemperaturesstart =  micros();
        //sensors.setWaitForConversion(false);
        sensors.requestTemperatures();
        //sensors.setWaitForConversion(true);
        unsigned long requestTemperaturesstop =  micros();
        bDalasstarted = true;
        startMills = millis();

        /*
        if (DEBUG_PRINT) {
          syslogPayload = "requestTemperatures delay : ";
          syslogPayload += (requestTemperaturesstop - requestTemperaturesstart) ;
          sendUdpSyslog(syslogPayload);
        }
        */
      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}

void runTimerDoLightOff() {
  if (( relaystatus == HIGH ) && ( hour() == 6 ) && ( minute() == 00 ) && ( second() < 5 ))
  {
    if (INFO_PRINT) {
      syslogPayload = "changing => relaystatus => runTimerLighrOff";
      syslogPayload += relaystatus;
      sendUdpSyslog(syslogPayload);
    }
    relaystatus = LOW;
  }
}

void changelight() {
  if (INFO_PRINT) {
    syslogPayload = "checking => relaystatus => change light ";
    syslogPayload += relaystatus;
    sendUdpSyslog(syslogPayload);
  }

  relayIsReady = LOW;
  digitalWrite(RELAYPIN, relaystatus);
  //delay(50);

  if (INFO_PRINT) {
    syslogPayload = "changing => relaystatus => ";
    syslogPayload += relaystatus;
    sendUdpSyslog(syslogPayload);
  }

  lastRelayActionmillis = millis();
  oldrelaystatus = relaystatus ;
  //relayIsReady = LOW;
}

void ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payload) {
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, 1)) {
    /*
    if (INFO_PRINT) {
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish ok";
      sendUdpSyslog(syslogPayload);
    }
    */
    free(p);
  } else {
    if (DEBUG_PRINT) {
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish fail";
      sendUdpSyslog(syslogPayload);
    }
    free(p);
  }
  client.loop();
}

void run_lightcmd() {
  if ( relayIsReady == HIGH  ) {
    relaystatus = !relaystatus;
  }
}

// pin 16 can't be used for Interrupts

void ICACHE_RAM_ATTR sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-sw-temp: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR sendUdpmsg(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

String macToStr(const uint8_t* mac) {
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

time_t getNtpTime() {
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(time_server);
  delay(3000);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0;
}

void sendNTPpacket(IPAddress & address) {
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
}


long DateToMjd (uint16_t y, uint8_t m, uint8_t d) {
  return
    367 * y
    - 7 * (y + (m + 9) / 12) / 4
    - 3 * ((y + (m - 9) / 7) / 100 + 1) / 4
    + 275 * m / 9
    + d
    + 1721028
    - 2400000;
}

static unsigned long  numberOfSecondsSinceEpochUTC(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s) {
  long Days;
  Days = DateToMjd(y, m, d) - DateToMjd(1970, 1, 1);
  return (uint16_t)Days * 86400 + h * 3600L + mm * 60L + s - (timeZone * SECS_PER_HOUR);
}


//
