// esp-01 1M / 64K flash / esp-solar
#include <TimeLib.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

extern "C" {
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"

#define INFO_PRINT 0
#define DEBUG_PRINT 1

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

// ****************

time_t getNtpTime();
String macToStr(const uint8_t* mac);
void callback(char* intopic, byte* inpayload, unsigned int length);
void sendmqttMsg(char* topictosend, String payload);
void sendNTPpacket(IPAddress & address);
void sendUdpSyslog(String msgtosend);
void sendUdpmsg(String msgtosend);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

//
IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

struct {
  float Temperature1;
  float Temperature2;
  float Humidity;
  float data1;
  float data2;
  uint16_t powerAvg;
  uint16_t WeightAvg;
  uint16_t pir;
} solar_data;

char* hellotopic  = "HELLO";

char* willTopic   = "clients/solar";
char* willMessage = "0";

// subscribe
const char subrpi[]     = "raspberrypi/data";
const char subtopic_0[] = "esp8266/arduino/s03"; // lcd temp
const char subtopic_1[] = "esp8266/arduino/s07"; // power
const char subtopic_2[] = "esp8266/arduino/s06"; // nemo scale
const char subtopic_3[] = "radio/test/2";        //Outside temp
const char subtopic_4[] = "raspberrypi/doorpir";

const char* substopic[6] = { subrpi, subtopic_0, subtopic_1, subtopic_2, subtopic_3, subtopic_4 } ;

unsigned int localPort = 12390;
const int timeZone = 9;

//
String clientName;
String payload;

String syslogPayload;

// send reset info
String getResetInfo;
int ResetInfo = LOW;

/////////////
LiquidCrystal_I2C lcd(0x27, 20, 4);
WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
WiFiUDP udp;

long lastReconnectAttempt = 0;
volatile bool msgcallback;

//
volatile uint32_t lastTime;

// https://omerk.github.io/lcdchargen/
byte termometru[8]      = { B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110 };
byte picatura[8]        = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110 };
byte pirfill[8]         = { B00111, B00111, B00111, B00111, B00111, B00111, B00111, B00111 };
byte powericon[8]       = { B11111, B11011, B10001, B11011, B11111, B11000, B11000, B11000 };
byte nemoicon[8]        = { B11011, B11011, B00100, B11111, B10101, B11111, B01010, B11011 };
byte customCharB[8]     = { B11110, B10001, B10001, B11110, B11110, B10001, B10001, B11110 };
byte customCharfill[8]  = { B10101, B01010, B10001, B00100, B00100, B10001, B01010, B10101 };

void wifi_connect() {
  /*
    wifi_set_phy_mode(PHY_MODE_11N);
    system_phy_set_max_tpw(10);
  */

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.hostname("esp-solar");

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
        client.publish(hellotopic, "hello again 1 from solar");
      }

      client.loop();
      for (int i = 0; i < 6; ++i) {
        client.subscribe(substopic[i]);
        client.loop();
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
  parseMqttMsg(receivedpayload, receivedtopic);
}

void parseMqttMsg(String receivedpayload, String receivedtopic) {
  char json[] = "{\"Humidity\":43.90,\"Temperature\":22.00,\"DS18B20\":22.00,\"PIRSTATUS\":0,\"FreeHeap\":43552,\"acquireresult\":0,\"acquirestatus\":0,\"DHTnextSampleTime\":2121587,\"bDHTstarted\":0,\"RSSI\":-48,\"millis\":2117963}";

  receivedpayload.toCharArray(json, 400);
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    return;
  }

  if ( receivedtopic == substopic[0] ) {
    if (root.containsKey("data1")) {
      solar_data.data1 = root["data1"];
    }
    if (root.containsKey("data2")) {
      solar_data.data2 = root["data2"];
    }
    lastTime = millis();
  }

  if ( receivedtopic == substopic[1] ) {
    if (root.containsKey("Humidity")) {
      solar_data.Humidity = root["Humidity"];
    }

    if (root.containsKey("Temperature")) {
      solar_data.Temperature1 = root["Temperature"];
    }
  }

  if ( receivedtopic == substopic[2] ) {
    if (root.containsKey("powerAvg")) {
      solar_data.powerAvg = root["powerAvg"];
    }
  }

  if ( receivedtopic == substopic[3] ) {
    if (root.containsKey("WeightAvg")) {
      solar_data.WeightAvg = root["WeightAvg"];
    }
  }

  if ( receivedtopic == substopic[4] ) {
    if (root.containsKey("data1")) {
      solar_data.Temperature2 = root["data1"];
    }
  }

  if ( receivedtopic == substopic[5] ) {
    if (root.containsKey("DOORPIR")) {
      if (solar_data.pir != root["DOORPIR"]) {
        solar_data.pir  = int(root["DOORPIR"]);
      }
    }
  }

  msgcallback = !msgcallback;
}

void setup() {
  Serial.swap();
  system_update_cpu_freq(SYS_CPU_80MHz);
  Wire.begin(0, 2);

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  //
  lastReconnectAttempt = 0;
  msgcallback = false;

  getResetInfo = "hello from solar ";
  getResetInfo += ESP.getResetInfo().substring(0, 50);

  //
  solar_data.Temperature1 = solar_data.Temperature2 = solar_data.Humidity = solar_data.data1 = solar_data.data2 = solar_data.powerAvg = solar_data.WeightAvg = 0;
  lastTime = millis();

  // lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.createChar(1, termometru);
  lcd.createChar(2, picatura);
  lcd.createChar(5, pirfill);
  lcd.createChar(6, powericon);
  lcd.createChar(7, nemoicon);

  lcd.createChar(3, customCharB);
  lcd.createChar(4, customCharfill);

  for ( int i = 1 ; i < 19 ; i++) {
    lcd.setCursor(i, 0);
    lcd.write(4);
    lcd.setCursor(i, 3);
    lcd.write(4);
  }

  for ( int i = 0 ; i < 4 ; i++) {
    lcd.setCursor(0, i);
    lcd.write(4);
    lcd.setCursor(19, i);
    lcd.write(4);
  }

  WiFiClient::setLocalPortStart(analogRead(A0));
  wifi_connect();

  udp.begin(localPort);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    setSyncProvider(getNtpTime);
  }

  //OTA
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-solar");
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

  if (DEBUG_PRINT) {
    syslogPayload = "------------------> solar started";
    sendUdpSyslog(syslogPayload);
  }

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.write(1);

  lcd.setCursor(0, 2);
  lcd.write(2);

  lcd.setCursor(8, 2);  // power
  lcd.write(6);

  lcd.setCursor(0, 3);  // nemo
  lcd.write(7);

  lcd.setCursor(8, 3);  // b
  lcd.write(3);

  //
  lcd.setCursor(6, 1);
  lcd.print((char)223);

  lcd.setCursor(12, 1);
  lcd.print((char)223);

  lcd.setCursor(6, 2);
  lcd.print("%");

}

time_t prevDisplay = 0;

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
      if (timeStatus() != timeNotSet) {
        if (now() != prevDisplay) {
          prevDisplay = now();
          digitalClockDisplay();
          displayTemperature();
          displayNemoWeight();
          displaypowerAvg();
          displayData();
          displaypir();

          if (msgcallback) {
            lcd.setCursor(19, 0);
            lcd.write(5);
          } else {
            lcd.setCursor(19, 0);
            lcd.print(" ");
          }

          if (( millis() - lastTime ) > 120000 ) {
            lcd.setCursor(19, 3);
            lcd.write(5);
          } else {
            lcd.setCursor(19, 3);
            lcd.print(" ");
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

void displayData() {
  lcd.setCursor(10, 3);
  if ( solar_data.data1 < 1000 ) {
    lcd.print(" ");
  }
  lcd.print(solar_data.data1, 0);

  lcd.setCursor(15, 3);
  lcd.print(solar_data.data2, 2);
}


void displaypir() {
  if ( solar_data.pir == 1) {
    for ( int i = 0 ; i <= 2 ; i ++ ) {
      lcd.setCursor(19, i);
      lcd.write(5);
    }
  } else {
    for ( int i = 0 ; i <= 2 ; i ++ ) {
      lcd.setCursor(19, i);
      lcd.print(" ");
    }
  }
}

void displaypowerAvg() {
  String str_Power = String(solar_data.powerAvg);
  int length_Power = str_Power.length();

  lcd.setCursor(10, 2);
  for ( int i = 0; i < ( 4 - length_Power ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_Power);
}

void displayNemoWeight() {
  String str_nemoWeight = String(solar_data.WeightAvg);
  int length_nemoWeight = str_nemoWeight.length();

  lcd.setCursor(2, 3);

  for ( int i = 0; i < ( 4 - length_nemoWeight ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_nemoWeight);
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
  displayTemperaturedigit(solar_data.Temperature1);

  lcd.setCursor(7, 1);

  float tempdiff = solar_data.Temperature2 - solar_data.Temperature1 ;
  displayTemperaturedigit(solar_data.Temperature2);

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

  lcd.setCursor(2, 2);
  if ( solar_data.Humidity >= 10 ) {
    lcd.print(solar_data.Humidity, 1);
  } else {
    lcd.print(" ");
    lcd.print(solar_data.Humidity, 1);
  }
}
void digitalClockDisplay() {
  // digital clock display of the time
  lcd.setCursor(0, 0);
  printDigitsnocolon(month());
  lcd.print("/");
  printDigitsnocolon(day());

  lcd.setCursor(6, 0);
  lcd.print(dayShortStr(weekday()));
  lcd.setCursor(10, 0);
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

void ICACHE_RAM_ATTR sendUdpSyslog(String msgtosend) {
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("mqtt-solar: ");
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
