// 80M CPU / 4M / 1M SPIFFS / esp-swtemp
/*
  D1(TX)    - DHT22(NEW)
  D3(RX)    - nrf24l01
  D5(SCL)   - TOP BUTTON
  D4(SDA)   - RELAY
  D0        - DS18B20
  D2        - DHT22(OLD), not used
  D15(SS)   - nrf24l01
  D13(MOSI) - nrf24l01
  D12(MISO) - nrf24l01
  D14(SCK)  - nrf24l01
  D16       - PIR
  ADC
*/
#include <TimeLib.h>
//#include <TimeAlarms.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <OneWire.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <DallasTemperature.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// radio
#define DEVICE_ID 1
#define CHANNEL 100 // MAX 127

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

extern "C" {
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"

//#define ENABLE_DHT

#if defined(ENABLE_DHT)
#include "PietteTech_DHT.h"
#define DHT_DEBUG_TIMING
#endif

#define INFO_PRINT 0
#define DEBUG_PRINT 1

// ****************

time_t getNtpTime();
String macToStr(const uint8_t* mac);
void callback(char* intopic, byte* inpayload, unsigned int length);
void run_lightcmd();
void changelight();
void sendmqttMsg(char* topictosend, String payload);
void runTimerDoLightOff();
void sendNTPpacket(IPAddress & address);
void sendUdpSyslog(String msgtosend);
void printEdgeTiming(class PietteTech_DHT *_d);
void dht_wrapper();
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
#define RELAYPIN 4
#define TOPBUTTONPIN 5

// OTHER
#define REPORT_INTERVAL 5000 // in msec
#define BETWEEN_RELAY_ACTIVE 5000

// DHT22
#define DHTTYPE  DHT22          // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   1              // Digital pin for communications

// DS18B20
#define ONE_WIRE_BUS 0
#define TEMPERATURE_PRECISION 9

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress outsideThermometer;

// radio
RF24 radio(3, 15);

// Topology
const uint64_t pipes[4] = {   0xFFFFFFFFFFLL,   0xCCCCCCCCCCLL,   0xFFFFFFFFCCLL,   0xFFFFFFFFCDLL  };
//  radio.openReadingPipe(1, pipes[0]); -->  5 : door, 65 : roll, 2 : DS18B20
//  radio.openReadingPipe(2, pipes[2]); --> 15 : ads1115
//  radio.openReadingPipe(3, pipes[3]); --> 25 : lcd

const uint32_t ampereunit[]  = { 0, 1000000, 1, 1000};

struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t data1;
  int16_t data2;
  uint8_t devid;
} sensor_data;

struct {
  uint32_t timestamp;
  float data1;
  float data2;
} data_ackpayload;

struct {
  uint32_t timestamp;
} time_reqpayload;

// mqtt
char* topic       = "esp8266/arduino/s02";
char* subtopic    = "esp8266/cmd/light";
char* rslttopic   = "esp8266/cmd/light/rlst";
char* hellotopic  = "HELLO";
char* radiotopic  = "radio/test";
char* radiofault  = "radio/test/fault";

char* willTopic   = "clients/relay";
char* willMessage = "0";

char* subrpi      = "raspberrypi/data";

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
uint32_t timestamp;
int millisnow;

//
int relayIsReady = HIGH;


// DHT

//declaration

#if defined(ENABLE_DHT)
// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
bool bDHTstarted;       // flag to indicate we started acquisition
int acquireresult;
int acquirestatus;
int _sensor_error_count;
unsigned long _sensor_report_count;
float t, h;
#endif

// ds18b20
bool bDalasstarted;

/////////////
WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
WiFiUDP udp;

long lastReconnectAttempt = 0;

#if defined(ENABLE_DHT)
// This wrapper is in charge of calling
// must be defined like this for the lib work
void ICACHE_RAM_ATTR dht_wrapper() {
  DHT.isrCallback();
}
#endif

void wifi_connect() {
  //wifi_set_phy_mode(PHY_MODE_11N);
  //wifi_set_channel(channel);

  //WiFiClient::setLocalPortStart(micros());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-swtemp");


  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300) {
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
      client.loop();
      client.subscribe(subtopic);
      client.loop();
      client.subscribe(subrpi);
      client.loop();

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

  if (DEBUG_PRINT) {
    syslogPayload = intopic;
    syslogPayload += " ====> ";
    syslogPayload += receivedpayload;
    sendUdpSyslog(syslogPayload);
  }

  if ( receivedtopic == "esp8266/cmd/light" ) {

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
  } else if ( receivedtopic == "raspberrypi/data" ) {
    char json[] = "{\"Humidity\":43.90,\"Temperature\":22.00}";

    receivedpayload.toCharArray(json, 150);
    StaticJsonBuffer<150> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(json);

    if (!root.success()) {
      return;
    }

    if (root.containsKey("data1")) {
      data_ackpayload.data1 = root["data1"];
      data_ackpayload.data2 = root["data2"];
    }
  }
}

void setup() {
  system_update_cpu_freq(SYS_CPU_80MHz);
  // wifi_status_led_uninstall();

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

  data_ackpayload.timestamp = data_ackpayload.data1 = data_ackpayload.data2 = 0;
  time_reqpayload.timestamp = 0;

  getResetInfo = "hello from ESP8266 s02 ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    setSyncProvider(getNtpTime);
  }

  attachInterrupt(5, run_lightcmd, CHANGE);

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

  //if ( isnan(tempCoutside) ) {
  if ( tempCoutside < -30 ) {
    if (INFO_PRINT) {
      sendUdpSyslog("Failed to read from DS18B20 sensor!");
    }
  }

  // radio
  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_1MBPS);
  //radio.setDataRate(RF24_2MBPS);
  radio.setRetries(15, 15);
  //
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  /*
    radio.enableDynamicAck();
    radio.enableAckPayload();
    radio.enableDynamicPayloads();
  */
  //radio.maskIRQ(1, 1, 0);
  //radio.openWritingPipe(pipes[1]);
  //
  radio.openReadingPipe(1, pipes[0]);
  radio.openReadingPipe(2, pipes[2]);
  radio.openReadingPipe(3, pipes[3]);
  radio.startListening();

  //OTA
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
    if (error == OTA_AUTH_ERROR) abort();
    else if (error == OTA_BEGIN_ERROR) abort();
    else if (error == OTA_CONNECT_ERROR) abort();
    else if (error == OTA_RECEIVE_ERROR) abort();
    else if (error == OTA_END_ERROR) abort();

  });

  ArduinoOTA.begin();

#if defined(ENABLE_DHT)
  // dht22
  _sensor_error_count = _sensor_report_count = 0;
  acquireresult = DHT.acquireAndWait(100);
  if (acquireresult != 0) {
    _sensor_error_count++;
  }
  if ( acquireresult == 0 ) {
    t = DHT.getCelsius();
    h = DHT.getHumidity();
  } else {
    t = h = 0;
  }
#endif

  if (DEBUG_PRINT) {
    syslogPayload = "------------------> unit started : pin 2 status : ";
    syslogPayload += digitalRead(2);
    sendUdpSyslog(syslogPayload);
  }

  // turn off light at 6
  //Alarm.alarmRepeat(6, 0, 0, runTimerDoLightOff); // 8:30am every day
}

time_t prevDisplay = 0;

void loop() {
  /*
    if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) {
      prevDisplay = now();
      //do_thing();
    }
    }
  */

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

#if defined(ENABLE_DHT)
      if (bDHTstarted) {
        acquirestatus = DHT.acquiring();
        if (!acquirestatus) {
          acquireresult = DHT.getStatus();
#if defined(DHT_DEBUG_TIMING)
          printEdgeTiming(&DHT);
#endif
          if ( acquireresult == 0 ) {
            t = DHT.getCelsius();
            h  = DHT.getHumidity();
          }
          bDHTstarted = false;
        }
      }
#endif

      if (bDalasstarted) {
        if (millis() > (startMills + (750 / (1 << (12 - TEMPERATURE_PRECISION))))) {
          unsigned long getTempCstart =  micros();
          tempCoutside  = sensors.getTempC(outsideThermometer);
          unsigned long getTempCstop =  micros();
          bDalasstarted = false;
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

      if ((relayIsReady == LOW ) &&  ( millis() < 15000 ) && ( relaystatus == oldrelaystatus )) {

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

      } else if ((relayIsReady == LOW ) &&  (( millis() - lastRelayActionmillis) > BETWEEN_RELAY_ACTIVE ) && ( relaystatus == oldrelaystatus )) {

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

      payload = "{\"PIRSTATUS\":";
      payload += pirValue;

#if defined(ENABLE_DHT)
      payload += ",\"Humidity\":";
      payload += h;
      payload += ",\"Temperature\":";
      payload += t;

      payload += ",\"acquireresult\":";
      payload += acquireresult;
      payload += ",\"acquirestatus\":";
      payload += acquirestatus;
#endif

      if ( tempCoutside > -30 ) {
        payload += ",\"DS18B20\":";
        payload += tempCoutside;
      }

      payload += ",\"FreeHeap\":";
      payload += ESP.getFreeHeap();
      payload += ",\"RSSI\":";
      payload += WiFi.RSSI();
      payload += ",\"millis\":";
      payload += (millis() - timemillis);
      payload += "}";

      if ( pirSent == HIGH ) {
        sendmqttMsg(topic, payload);
        pirSent = LOW;
      }

      //  radio.openReadingPipe(1, pipes[0]); -->  5 : door, 65 : roll, 2 : DS18B20
      //  radio.openReadingPipe(2, pipes[2]); --> 15 : ads1115
      //  radio.openReadingPipe(2, pipes[3]); --> 25 : lcd
      byte pipeNo;
      if (radio.available(&pipeNo)) {
        // from attiny 85 data size is 11
        // sensor_data data size = 12
        uint8_t len = radio.getDynamicPayloadSize();
        // avr 8bit, esp 32bit. esp use 4 byte step.

        // use switch ?
        if (len == sizeof(time_reqpayload)) {
          data_ackpayload.timestamp = now();

          radio.writeAckPayload(pipeNo, &data_ackpayload, sizeof(data_ackpayload));
          radio.read(&time_reqpayload, sizeof(time_reqpayload));

          if (DEBUG_PRINT) {
            syslogPayload = data_ackpayload.timestamp;
            syslogPayload += " ==> ";
            syslogPayload += data_ackpayload.data1;
            syslogPayload += " ==> ";
            syslogPayload += data_ackpayload.data2;
            sendUdpSyslog(syslogPayload);
          }

        } else if ((len + 1 ) == sizeof(sensor_data)) {
          radio.read(&sensor_data, sizeof(sensor_data));
          if ( (pipeNo == 1 || pipeNo == 3 ) && sensor_data.devid != 15 ) {

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

            if ( (sensor_data.devid > 0) && (sensor_data.devid < 255) ) {
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
          } else if ( pipeNo == 2 && sensor_data.devid == 15 ) {
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
            // UTC
            udppayload += (now() - timeZone * SECS_PER_HOUR);
            char buf[3];
            sprintf(buf, "%03d", millisecond());
            udppayload += buf;
            udppayload += "000000";
            sendUdpmsg(udppayload);
          }
        } else {
          radio.read(0, 0);
        }
      }

      if ((millis() - startMills) > REPORT_INTERVAL ) {
        sendmqttMsg(topic, payload);

        sensors.requestTemperatures();
        bDalasstarted = true;

#if defined(ENABLE_DHT)
        if (acquirestatus == 1) {
          DHT.reset();
        }

        if (!bDHTstarted) {
          DHT.acquire();
          bDHTstarted = true;
        }
#endif
        startMills = millis();
      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
  //Alarm.delay(1);
}

void runTimerDoLightOff() {
  if (( relaystatus == HIGH ) && ( hour() == 6 ) && ( minute() == 00 ) && ( second() < 5 )) {
    if (INFO_PRINT) {
      syslogPayload = "changing => relaystatus => runTimerLightOff";
      syslogPayload += relaystatus;
      sendUdpSyslog(syslogPayload);
    }
    relaystatus = LOW;

    /* need to inform mqtt, when light is off.
      String lightpayload = "{\"LIGHT\":";
      lightpayload += relaystatus;
      lightpayload += "}";

      sendmqttMsg(subtopic, lightpayload);
    */
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

  if (INFO_PRINT) {
    syslogPayload = "changing => relaystatus => ";
    syslogPayload += relaystatus;
    sendUdpSyslog(syslogPayload);
  }

  lastRelayActionmillis = millis();
  oldrelaystatus = relaystatus ;
}

void ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payload) {
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, 1)) {
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

#if defined(ENABLE_DHT)
void ICACHE_RAM_ATTR printEdgeTiming(class PietteTech_DHT * _d) {
  byte n;
#if defined(DHT_DEBUG_TIMING)
  volatile uint8_t *_e = &_d->_edges[0];
#endif
  int result = _d->getStatus();
  if (result != 0) {
    _sensor_error_count++;
  }

  _sensor_report_count++;

  String udppayload = "edges2,device=esp-12-N2,debug=on,DHTLIB_ONE_TIMING=110 ";
  for (n = 0; n < 41; n++) {
    char buf[2];
    udppayload += "e";
    sprintf(buf, "%02d", n);
    udppayload += buf;
    udppayload += "=";
#if defined(DHT_DEBUG_TIMING)
    udppayload += *_e++;
#endif
    udppayload += "i,";
  }
  udppayload += "F=";
  udppayload += ESP.getCpuFreqMHz();
  udppayload += "i,C=";
  udppayload += _sensor_report_count;
  udppayload += "i,R=";
  udppayload += result;
  udppayload += ",E=";
  udppayload += _sensor_error_count;
  udppayload += "i,H=";
  udppayload += _d->getHumidity();
  udppayload += ",T=";
  udppayload += _d->getCelsius();

  sendUdpmsg(udppayload);
}
#endif

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
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500) {
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

