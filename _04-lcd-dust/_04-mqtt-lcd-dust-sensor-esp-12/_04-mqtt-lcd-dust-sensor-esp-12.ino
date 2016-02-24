// 80MHz / 4M / 1M / ESP-01 (flash chip is changed) / esp-lcd
#include <TimeLib.h>
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <Wire.h>
#include <PubSubClient.h>
#include "PietteTech_DHT.h"
/* rtc
  #include <RtcDS1307.h>
*/

#define DHT_DEBUG_TIMING

#define REPORT_INTERVAL 3000 // in msec

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

//
void callback(char* intopic, byte* inpayload, unsigned int length);
void parseMqttMsg(String receivedpayload, String receivedtopic);
String macToStr(const uint8_t* mac);
time_t getNtpTime();
void displaysleepmode(int sleepmode);
void displayHost(int numofhost, int numofall);
void displaypowerAvg(float Power);
void displayNemoWeight(int nemoWeight);
void displayPIR();
void displayTemperaturedigit(float Temperature);
void displayTemperature();
void displaydustDensity();
void printDigitsnocolon(int digits);
void printDigits(int digits);
void sendNTPpacket(IPAddress & address);
void digitalClockDisplay();
void requestSharp();
void sendmqttMsg(String payloadtosend);
void printEdgeTiming(class PietteTech_DHT *_d);
void sendUdpmsg(String msgtosend);
void sendUdpSyslog(String msgtosend);
void printSquareWaveCount();

//
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

//
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

//
#define INFO_PRINT 0
#define DEBUG_PRINT 0

// rtc

#define SquareWavePin 1
volatile unsigned long SquareWaveCount;
unsigned long old_SquareWaveCount;

/* rtc
  RtcDS1307 Rtc;
*/

//---------
char* topic = "esp8266/arduino/s03";
char* hellotopic = "HELLO";

char* willTopic = "clients/dust";
char* willMessage = "0";

const char subtopic_0[] = "esp8266/arduino/s02";
const char subtopic_1[] = "radio/test/2";
const char subtopic_2[] = "esp8266/arduino/s07";
const char subtopic_3[] = "esp8266/arduino/s06";
const char subtopic_4[] = "raspberrypi/doorpir";
const char subtopic_5[] = "home/check/checkhwmny";

const char* substopic[6] = { subtopic_0, subtopic_1, subtopic_2, subtopic_3, subtopic_4, subtopic_5} ;

LiquidCrystal_I2C lcd(0x27, 20, 4);

unsigned int localPort = 2390;  // local port to listen for UDP packets
const int timeZone = 9;

String clientName;
String payload;
String syslogPayload;

WiFiClient wifiClient;
WiFiUDP udp;

// send reset info
String getResetInfo ;
int ResetInfo = LOW;

PubSubClient client(mqtt_server, 1883, callback, wifiClient);

volatile float H, T1, T2, OT, PW;
volatile int NW, PIR, HO, HL, unihost, rsphost, unitot, rsptot;
volatile bool msgcallback;

int sleepmode = LOW ;
int o_sleepmode = LOW ;

float dustDensity ;
int moisture ;

unsigned long startMills;
unsigned long sentMills ;

long lastReconnectAttempt = 0;

byte termometru[8]      = { B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110, };
byte picatura[8]        = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110, };
byte dustDensityicon[8] = { B11111, B11111, B11011, B10001, B10001, B11011, B11111, B11111, };
byte dustDensityfill[8] = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111, };
byte pirfill[8]         = { B00111, B00111, B00111, B00111, B00111, B00111, B00111, B00111, };
byte powericon[8]       = { B11111, B11011, B10001, B11011, B11111, B11000, B11000, B11000, };
byte nemoicon[8]        = { B11011, B11011, B00100, B11111, B10101, B11111, B01010, B11011, };

// system defines
#define DHTTYPE  DHT22        // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   3            // Digital pin for communications

//declaration
void ICACHE_RAM_ATTR dht_wrapper(); // must be declared before the lib initialization

// Lib instantiate
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

// globals
bool bDHTstarted;       // flag to indicate we started acquisition
int acquireresult;
int acquirestatus;
int _sensor_error_count;
unsigned long _sensor_report_count;

// This wrapper is in charge of calling
// must be defined like this for the lib work
void ICACHE_RAM_ATTR dht_wrapper() {
  DHT.isrCallback();
}

void callback(char* intopic, byte* inpayload, unsigned int length) {
  String receivedtopic = intopic;
  String receivedpayload ;

  if ( receivedtopic == "esp8266/arduino/s03" || receivedtopic == "clients/dust" ) {
    return;
  }

  for (int i = 0; i < length; i++) {
    receivedpayload += (char)inpayload[i];
  }
  parseMqttMsg(receivedpayload, receivedtopic);
}

void parseMqttMsg(String receivedpayload, String receivedtopic) {
  //char json[] = "{\"VIrms\":595,\"revValue\":718.56,\"revMills\":8350,\"powerAvg\":656.78,\"Stddev\":66.70,\"calcIrmsmillis\":153,\"revCounts\":126,\"FreeHeap\":46336,\"RSSI\":-61,\"millis\":1076571}";
  char json[] = "{\"Humidity\":43.90,\"Temperature\":22.00,\"DS18B20\":22.00,\"PIRSTATUS\":0,\"FreeHeap\":43552,\"acquireresult\":0,\"acquirestatus\":0,\"DHTnextSampleTime\":2121587,\"bDHTstarted\":0,\"RSSI\":-48,\"millis\":2117963}";

  receivedpayload.toCharArray(json, 400);
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    return;
  }

  // topic
  // raspberrypi/doorpir
  // esp8266/arduino/s07 : power
  // esp8266/arduino/s04 : OUTSIDE
  // esp8266/arduino/s06 : Scale
  // esp8266/arduino/s02  : ds18b20
  // esp8266/arduino/aircon : ________
  // home/check/checkhwmny : unihost, rsphost, unitot, rsptot

  if ( receivedtopic == substopic[0] ) {
    if (root.containsKey("DS18B20")) {
      T2  = root["DS18B20"];
    }
  }

  if ( receivedtopic == substopic[1] ) {
    if (root.containsKey("data1")) {
      OT  = root["data1"];
    }
  }

  if ( receivedtopic == substopic[2] ) {
    if (root.containsKey("powerAvg")) {
      PW  = root["powerAvg"];
    }
  }

  if ( receivedtopic == substopic[3] ) {
    if (root.containsKey("WeightAvg")) {
      NW  = root["WeightAvg"];
    }
  }

  if ( receivedtopic == substopic[4] )
  {
    if (root.containsKey("DOORPIR")) {
      PIR = root["DOORPIR"];
    }
  }

  if ( receivedtopic == substopic[5] )
  {
    if (root.containsKey("unihost")) {
      const char* tempunihost =  root["unihost"].asString();
      unihost = atoi(tempunihost);
    }

    if (root.containsKey("rsphost")) {
      const char* temprsphost =  root["rsphost"].asString();
      rsphost = atoi(temprsphost);
    }

    if (root.containsKey("unitot")) {
      const char* tempunitot =  root["unitot"].asString();
      unitot = atoi(tempunitot);
    }

    if (root.containsKey("rsptot")) {
      const char* temprsptot =  root["rsptot"].asString();
      rsptot = atoi(temprsptot);
    }
  }
  msgcallback = !msgcallback;
}

void wifi_connect() {
  wifi_set_phy_mode(PHY_MODE_11N);
  //wifi_set_channel(channel);
  system_phy_set_max_tpw(1);

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
  if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage)) {
    client.publish(willTopic, "1", true);
    client.loop();
    if ( ResetInfo == LOW) {
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      ResetInfo = HIGH;
    } else {
      client.publish(hellotopic, "hello again 1 from lcd ");
    }

    client.loop();
    for (int i = 0; i < 6; ++i) {
      if (DEBUG_PRINT) {
        syslogPayload = "subscribed to : ";
        syslogPayload += i;
        syslogPayload += " - ";
        syslogPayload += substopic[i];
        sendUdpSyslog(syslogPayload);

        client.subscribe(substopic[i]);
        client.loop();
      } else {
        client.subscribe(substopic[i]);
        client.loop();
      }
    }

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
  return client.connected();
}

/* rtc
  void ICACHE_RAM_ATTR check_SquareWaveCount() {
  SquareWaveCount++;
  }
*/

void setup() {
  delay(20);

  Serial.swap();
  system_update_cpu_freq(SYS_CPU_80MHz);
  startMills = sentMills = millis();
  Wire.begin(0, 2);

  T2 =  OT = PW = NW =  dustDensity = SquareWaveCount = old_SquareWaveCount = 0 ;
  //T2 =  OT = PW = NW =  dustDensity = 0 ;
  PIR = HO = HL = moisture = unihost = rsphost = unitot = rsptot = 0;
  acquirestatus = 0;
  msgcallback = false;


  /*
    enum DS1307SquareWaveOut
    {
    DS1307SquareWaveOut_1Hz  =  0b00010000,
    DS1307SquareWaveOut_4kHz =  0b00010001,
    DS1307SquareWaveOut_8kHz =  0b00010010,
    DS1307SquareWaveOut_32kHz = 0b00010011,
    DS1307SquareWaveOut_High =  0b10000000,
    DS1307SquareWaveOut_Low =   0b00000000,
    };
  */
  /* rtc
    Rtc.Begin();
    Rtc.SetIsRunning(true);
    Rtc.SetSquareWavePin(DS1307SquareWaveOut_1Hz);
  */

  lastReconnectAttempt = 0;

  /* rtc
    pinMode(SquareWavePin, INPUT_PULLUP);
    attachInterrupt(SquareWavePin, check_SquareWaveCount, FALLING);
  */
  // call check_SquareWaveCount every loop, so not use ONLOW
  // attachInterrupt(SquareWavePin, check_SquareWaveCount, ONLOW);

  getResetInfo = "hello from ESP8266 lcd ";
  getResetInfo += ESP.getResetInfo().substring(0, 30);

  clientName += "esp8266 - ";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += " - ";
  clientName += String(micros() & 0xff, 16);

  WiFiClient::setLocalPortStart(analogRead(A0));
  wifi_connect();

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    setSyncProvider(getNtpTime);
  }

  // lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.createChar(1, termometru);
  lcd.createChar(2, picatura);
  lcd.createChar(3, dustDensityicon);
  lcd.createChar(4, dustDensityfill);
  lcd.createChar(5, pirfill);
  lcd.createChar(6, powericon);
  lcd.createChar(7, nemoicon);

  lcd.setCursor(0, 1);
  lcd.write(1);

  lcd.setCursor(0, 2);
  lcd.write(2);

  lcd.setCursor(8, 2);  // power
  lcd.write(6);

  lcd.setCursor(0, 3);  // nemo
  lcd.write(7);

  lcd.setCursor(8, 3); // dust
  lcd.write(3);

  //

  lcd.setCursor(6, 1);
  lcd.print((char)223);

  lcd.setCursor(12, 1);
  lcd.print((char)223);

  lcd.setCursor(6, 2);
  lcd.print("%");

  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-lcd");
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

  _sensor_error_count = _sensor_report_count = 0;
  acquireresult = DHT.acquireAndWait(100);
  if (acquireresult != 0) {
    _sensor_error_count++;
  }
  if ( acquireresult == 0 ) {
    T1 = DHT.getCelsius();
    H = DHT.getHumidity();
  } else {
    T1 = H = 0;
  }

  if (DEBUG_PRINT) {
    syslogPayload = "------------------> unit started : pin 1 status : ";
    syslogPayload += digitalRead(1);
    sendUdpSyslog(syslogPayload);
  }
}

time_t prevDisplay = 0; // when the digital clock was displayed

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
      /* rtc
        if ( SquareWaveCount > old_SquareWaveCount ) {
        printSquareWaveCount();
        old_SquareWaveCount = SquareWaveCount;
        }
      */

      if (bDHTstarted) {
        acquirestatus = DHT.acquiring();
        if (!acquirestatus) {
          acquireresult = DHT.getStatus();
#if defined(DHT_DEBUG_TIMING)
          printEdgeTiming(&DHT);
#endif
          if ( acquireresult == 0 ) {
            T1 = DHT.getCelsius();
            H  = DHT.getHumidity();
          }
          bDHTstarted = false;
        }
      }

      if (timeStatus() != timeNotSet) {
        if (now() != prevDisplay) { //update the display only if time has changed
          prevDisplay = now();
          digitalClockDisplay();
          requestSharp();
          HO = unihost + rsphost;
          HL = unitot + rsptot;

          displaysleepmode(sleepmode);
          displayHost(HO, HL);
          displaypowerAvg(PW);
          displayNemoWeight(NW);
          displayPIR();
          displayTemperature();
          displaydustDensity();

          if (msgcallback) {
            lcd.setCursor(19, 0);
            lcd.write(5);
          } else {
            lcd.setCursor(19, 0);
            lcd.print(" ");
          }
        }
      }

      if ((millis() - sentMills) > REPORT_INTERVAL ) {
        payload = "{\"dustDensity\":";
        payload += dustDensity;
        payload += ",\"moisture\":";
        payload += moisture;
        payload += ",\"Humidity\":";
        payload += H;
        payload += ",\"Temperature\":";
        payload += T1;

        /*
          payload += ",\"SquareWaveCount\":";
          payload += SquareWaveCount;
        */

        // to check DHT.acquiring()
        payload += ",\"acquireresult\":";
        payload += acquireresult;
        payload += ",\"acquirestatus\":";
        payload += acquirestatus;

        /*
          payload += DHT.acquiring();
        */
        payload += ",\"bDHTstarted\":";
        payload += bDHTstarted;
        payload += ",\"FreeHeap\":";
        payload += ESP.getFreeHeap();
        //
        payload += ",\"RSSI\":";
        payload += WiFi.RSSI();
        payload += ",\"millis\":";
        payload += millis();
        payload += "}";

        sendmqttMsg(payload);
        sentMills = millis();

        if (acquirestatus == 1) {
          DHT.reset();
        }

        if (!bDHTstarted) {
          DHT.acquire();
          bDHTstarted = true;
        }
      }
      client.loop();
    }
    ArduinoOTA.handle();
  } else {
    wifi_connect();
  }
}

void sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-lcd-dust: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void sendUdpmsg(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void printSquareWaveCount() {
  String udppayload = "SquareWave,device=esp-1 ";
  udppayload += " SquareWaveCount=";
  udppayload += SquareWaveCount;
  udppayload += "i";

  sendUdpmsg(udppayload);
}

void printEdgeTiming(class PietteTech_DHT *_d) {
  byte n;
#if defined(DHT_DEBUG_TIMING)
  volatile uint8_t *_e = &_d->_edges[0];
#endif
  int result = _d->getStatus();
  if (result != 0) {
    _sensor_error_count++;
  }

  _sensor_report_count++;

  String udppayload = "edges2,device=esp-12-N1,debug=on,DHTLIB_ONE_TIMING=110 ";
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

void displayHost(int numofhost, int numofall) {
  lcd.setCursor(15, 2);
  lcd.print(numofhost);

  lcd.setCursor(17, 2);
  if (numofall < 10) {
    lcd.print(' ');
  }
  lcd.print(numofall);
}

void displaysleepmode(int sleepmode) {
  if ( sleepmode == HIGH ) {
    lcd.setCursor(15, 2);
    lcd.write(3);
    lcd.setCursor(16, 2);
    lcd.write(3);
  } else {
    lcd.setCursor(15, 2);
    lcd.print(" ");
    lcd.setCursor(16, 2);
    lcd.print(" ");
  }
}

void displaypowerAvg(float Power) {
  String str_Power = String(int(Power));
  int length_Power = str_Power.length();

  lcd.setCursor(10, 2);
  for ( int i = 0; i < ( 4 - length_Power ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_Power);

}

void displayNemoWeight(int nemoWeight) {
  String str_nemoWeight = String(nemoWeight);
  int length_nemoWeight = str_nemoWeight.length();

  lcd.setCursor(2, 3);

  for ( int i = 0; i < ( 4 - length_nemoWeight ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_nemoWeight);
}

void displayPIR() {
  if ( PIR == 1) {
    for ( int i = 0 ; i <= 3 ; i ++ ) {
      lcd.setCursor(19, i);
      lcd.write(5);
    }
  } else {
    for ( int l = 0 ; l <= 3 ; l ++ ) {
      lcd.setCursor(19, l);
      lcd.print(" ");
    }
  }
}

void displayTemperaturedigit(float Temperature) {
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(Temperature, 1);
}

void displayTemperature() {
  lcd.setCursor(1, 1);
  displayTemperaturedigit((T1 + T2) / 2);

  lcd.setCursor(7, 1);

  if ( OT != -1000 ) {

    float tempdiff = OT - ((T1 + T2) / 2) ;
    displayTemperaturedigit(OT);

    lcd.setCursor(14, 1);
    if ( tempdiff > 0 ) {
      lcd.print("+");
    } else if ( tempdiff < 0 ) {
      lcd.print("-");
    }

    String str_tempdiff = String(int abs(tempdiff));
    int length_tempdiff = str_tempdiff.length();

    lcd.setCursor(15, 1);
    lcd.print(abs(tempdiff), 1);
    if ( length_tempdiff == 1) {
      lcd.print(" ");
    }
  }

  lcd.setCursor(2, 2);
  if ( H >= 10 ) {
    lcd.print(H, 1);
  } else {
    lcd.print(" ");
    lcd.print(H, 1);
  }
}

void displaydustDensity() {
  int n = int(dustDensity / 0.05) ;

  if ( n > 9 ) {
    n = 9 ;
  }

  for ( int i = 0 ; i < n ; i++) {
    lcd.setCursor(10 + i, 3);
    lcd.write(4);
  }

  for ( int o = 0 ; o < ( 9 - n) ; o++) {
    lcd.setCursor(10 + n + o, 3);
    lcd.print(".");
  }
}

void requestSharp() {
  Wire.requestFrom(2, 4);

  int x, y;
  byte a, b, c, d;

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();
  d = Wire.read();

  x = a;
  x = x << 8 | b;

  y = c;
  y = y << 8 | d;

  if ( x == 33333 ) {
    sleepmode = HIGH;
  }

  if ( x == 22222 ) {
    sleepmode = LOW;
  }

  if (( 1 < y ) && ( y < 1024 )) {
    moisture = y;
  }

  if (( 1 < x ) && ( x < 1024 )) {
    float calcVoltage = x * (5.0 / 1024.0);
    if ( (0.17 * calcVoltage - 0.1) > 0 ) {
      dustDensity = 0.17 * calcVoltage - 0.1;
    }
  }
}

void sendI2cMsg(byte a, byte b) {
  Wire.beginTransmission(2);
  Wire.write(a);
  Wire.write(b);
  Wire.endTransmission();
}

void sendmqttMsg(String payloadtosend) {
  unsigned int msg_length = payloadtosend.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payloadtosend.c_str(), msg_length);

  if ( client.publish(topic, p, msg_length, 1)) {
    if (INFO_PRINT) {
      syslogPayload = payloadtosend;
      syslogPayload += " : Publish ok";
      sendUdpSyslog(syslogPayload);
    }
    free(p);
  } else {
    if (INFO_PRINT) {
      syslogPayload = payloadtosend;
      syslogPayload += " : Publish fail";
      sendUdpSyslog(syslogPayload);
    }
    free(p);
  }
}

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5) {
      result += ':';
    }
  }
  return result;
}

void digitalClockDisplay() {
  // digital clock display of the time
  lcd.setCursor(0, 0);
  lcd.print(year());
  lcd.print("/");
  printDigitsnocolon(month());
  lcd.print("/");
  printDigitsnocolon(day());
  lcd.print(" ");
  printDigitsnocolon(hour());
  printDigits(minute());
  printDigits(second());
}

void printDigitsnocolon(int digits) {
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}


void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  lcd.print(":");
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime() {
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(time_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
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
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress & address) {
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
}
//
