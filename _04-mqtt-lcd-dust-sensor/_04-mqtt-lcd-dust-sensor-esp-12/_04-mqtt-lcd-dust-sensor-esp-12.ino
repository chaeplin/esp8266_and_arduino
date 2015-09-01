#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>
#include <RtcDS3231.h>

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <Time.h>


RtcDS3231 Rtc;

#define DEBUG_PRINT 1

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

const char* topic = "esp8266/arduino/s03";
const char* subtopic = "#";
const char* hellotopic = "HELLO";

LiquidCrystal_I2C lcd(0x27, 20, 4);

IPAddress server(192, 168, 10, 10);

unsigned int localPort = 2390;  // local port to listen for UDP packets
IPAddress timeServer(192, 168, 10, 10); // time.nist.gov NTP server
const int timeZone = 9;

String clientName;
String payload;
WiFiClient wifiClient;
WiFiUDP udp;

// volatile
float H  ;
float T1 ;
float T2 ;
float OT ;
float PW ;
int NW ;
int PIR  ;
int HO  ;
int HL;

int unihost;
int rsphost;
int unitot;
int rsptot;

int sleepmode = LOW ;
int o_sleepmode = LOW ;

float dustDensity ;

float OLD_H  ;
float OLD_T1 ;
float OLD_T2 ;
float OLD_OT ;
float OLD_PW ;
int OLD_NW ;
int OLD_PIR  ;
int OLD_HO  ;
int OLD_HL  ;

float OLD_dustDensity ;

int OLD_x ;

byte termometru[8] =
{
  B00100,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110,
};

byte picatura[8] =
{
  B00100,
  B00100,
  B01010,
  B01010,
  B10001,
  B10001,
  B10001,
  B01110,
};

byte dustDensityicon[8] =
{
  B11111,
  B11111,
  B11011,
  B10001,
  B10001,
  B11011,
  B11111,
  B11111,
};

byte dustDensityfill[8] =
{
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
};

byte pirfill[8] =
{
  B00111,
  B00111,
  B00111,
  B00111,
  B00111,
  B00111,
  B00111,
  B00111,
};

byte powericon[8] =
{
  B11111,
  B11011,
  B10001,
  B11011,
  B11111,
  B11000,
  B11000,
  B11000,
};

byte nemoicon[8] =
{
  B11011,
  B11011,
  B00100,
  B11111,
  B10101,
  B11111,
  B01010,
  B11011,
};


PubSubClient client(wifiClient, server);

void callback(const MQTT::Publish& pub) {
  if (1) {
    Serial.print(pub.topic());
    Serial.print(" => ");
    Serial.println(pub.payload_string());
  }

  String receivedtopic = pub.topic();
  parseMqttMsg(pub.payload_string(), receivedtopic);

}

void parseMqttMsg(String payload, String receivedtopic) {

  StaticJsonBuffer<300> jsonBuffer;

  char json[] =
    "{\"Humidity\":38.10,\"Temperature\":27.70,\"DS18B20\":28.31,\"SENSORTEMP\":31.31,\"PIRSTATUS\":1}";

  payload.toCharArray(json, 200);

  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }

  // topic
  // raspberrypi/doorpir
  // esp8266/arduino/s07 : power
  // esp8266/arduino/s04 : OUTSIDE
  // esp8266/arduino/s06 : Scale
  // esp8266/arduino/s02  : T, H
  // esp8266/arduino/aircon : ________
  // home/check/checkhwmny : unihost, rsphost, unitot, rsptot


  if ( receivedtopic == "esp8266/arduino/s02" ) {
    if (root.containsKey("Humidity")) {
      H   = root["Humidity"];
    }
    if (root.containsKey("Temperature")) {
      T1  = root["Temperature"];
    }
    if (root.containsKey("DS18B20")) {
      T2  = root["DS18B20"];
    }
  }

  if ( receivedtopic == "esp8266/arduino/s04" ) {
    if (root.containsKey("OUTSIDE")) {
      OT  = root["OUTSIDE"];
    }
  }

  if ( receivedtopic == "esp8266/arduino/s07" ) {
    if (root.containsKey("powerAvg")) {
      PW  = root["powerAvg"];
    }
  }

  if ( receivedtopic == "esp8266/arduino/s06" ) {
    if (root.containsKey("WeightAvg")) {
      int temp_NW = root["WeightAvg"];
      if ( temp_NW > 0 ) {
        NW  = root["WeightAvg"];
      }
    }
  }

  if ( receivedtopic == "raspberrypi/doorpir" )
  {
    if (root.containsKey("DOORPIR")) {
      PIR = root["DOORPIR"];
    }
  }

  if ( receivedtopic == "home/check/checkhwmny" )
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
}

void setup() {
  Serial.begin(38400);
  Rtc.Begin();
  Wire.pins(0, 2);
  Serial.println("LCD START");
  delay(20);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  #ifdef __IS_MY_HOME
  WiFi.config(IPAddress(192, 168, 10, 12), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
  #endif

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
    }
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  //
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  delay(500);
  setSyncProvider(getNtpTime);

  if (timeStatus() == timeNotSet) {
    Serial.println("waiting for sync message");
  }

  RtcDateTime compiled = now();
  Serial.println(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid())
  {
    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);

  //
  client.set_callback(callback);

  clientName += "esp8266 - ";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += " - ";
  clientName += String(micros() & 0xff, 16);

  Serial.print("Connecting to ");
  Serial.print(server);
  Serial.print(" as ");
  Serial.println(clientName);

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

  /*
    lcd.setCursor(14, 2);
    lcd.write(8);
  */

  H  = -1000 ;
  T1 = -1000 ;
  T2 = -1000 ;
  OT = -1000 ;
  PW = -1000 ;
  NW = -1000 ;
  PIR = 0 ;
  dustDensity = -1000 ;
  HO = 0 ;
  HL = 0;
  
  unihost = 0;
  rsphost = 0;
  
  unitot = 0;
  rsptot = 0;

  OLD_H  = -1000 ;
  OLD_T1 = -1000 ;
  OLD_T2 = -1000 ;
  OLD_OT = -1000 ;
  OLD_PW = -1000 ;
  OLD_NW = -1000 ;
  OLD_PIR = 0 ;
  OLD_dustDensity = -1000 ;
  OLD_HO = 0;
  OLD_HL = 0;

  OLD_x = 0 ;

}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  if (timeStatus() != timeNotSet) {
    
    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        if  (
          client.connect(MQTT::Connect((char*) clientName.c_str()).set_clean_session().set_keepalive(120))) {
          client.subscribe(subtopic);
          client.publish(hellotopic, "hello from ESP8266 s03");
          client.set_callback(callback);
        }
      }

      if (client.connected()) {
        client.loop();
      } else {
        ESP.restart();
      }
    } else {
        Serial.println("Could not connect to WIFI");
        ESP.restart();    
    }

    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      if ( ( second() % 3 ) == 0 ) {
        requestSharp();
      }
      /*
      if ( ( second() % 50 ) == 0 ) {
        client.subscribe(subtopic);
      }
      */
      checkDisplayValue();
    }
  }
}

void checkDisplayValue() {
  if ( sleepmode != o_sleepmode )
  {
    displaysleepmode(sleepmode);
    o_sleepmode = sleepmode;
  }

  HO = unihost + rsphost;
  HL = unitot + rsptot;
  
  if (( HO != OLD_HO ) || ( HL != OLD_HL))
  {
    displayHost(HO, HL);
    OLD_HO = HO;
    OLD_HL = HL;
  }

  if (( PW != OLD_PW ) && ( 0 <= PW < 10000 ))
  {
    displaypowerAvg(PW);
    OLD_PW = PW;
  }

  if ( NW != OLD_NW )
  {
    displayNemoWeight(NW);
    OLD_NW = NW;
  }

  if ( PIR != OLD_PIR )
  {
    displayPIR();
    OLD_PIR = PIR;
  }

  if ((T1 != OLD_T1 ) || (T2 != OLD_T2 ) || (OT != OLD_OT) || ( H != OLD_H ))
  {
    displayTemperature();
    OLD_T1 = T1;
    OLD_T2 = T2;
    OLD_OT = OT;
    OLD_H = H;
  }

  if ( dustDensity != OLD_dustDensity )
  {
    senddustDensity();
    displaydustDensity();
    OLD_dustDensity = dustDensity ;
  }

  if (DEBUG_PRINT) {
    Serial.print("=====> ");
    Serial.print(T1);
    Serial.print(" ===> ");
    Serial.print(T2);
    Serial.print(" ===> ");
    Serial.print(OT);
    Serial.print(" ===> ");
    Serial.print(H);
    Serial.print(" ===> ");
    Serial.print(dustDensity);
    Serial.print(" ===> ");
    Serial.print(PIR);
    Serial.print(" ===> ");
    Serial.print(PW);
    Serial.print(" ===> ");
    Serial.print(HO);
    Serial.print(" ===> ");
    Serial.println(NW);
  }
}

void displayHost(int numofhost, int numofall)
{
  lcd.setCursor(15, 2);
  lcd.print(numofhost);

  lcd.setCursor(17, 2);
    if (numofall < 10) {
    lcd.print(' ');
  }
  lcd.print(numofall);
}

void displaysleepmode(int sleepmode)
{
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

void displaypowerAvg(float Power)
{
  String str_Power = String(int(Power));
  int length_Power = str_Power.length();

  lcd.setCursor(10, 2);
  for ( int i = 0; i < ( 4 - length_Power ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_Power);

}

void displayNemoWeight(int nemoWeight)
{
  String str_nemoWeight = String(nemoWeight);
  int length_nemoWeight = str_nemoWeight.length();

  lcd.setCursor(2, 3);

  for ( int i = 0; i < ( 4 - length_nemoWeight ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_nemoWeight);
}

void displayPIR()
{
  if ( PIR == 1)
  {
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

void displayTemperaturedigit(float Temperature)
{
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(Temperature, 1);
}

void displayTemperature()
{
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


void displaydustDensity()
{

  int n = int(dustDensity / 0.05) ;

  if ( n > 9 ) {
    n = 9 ;
  }

  if (DEBUG_PRINT) {
    Serial.print(" ===> dustDensity ");
    Serial.print(dustDensity);
    Serial.print(" ===>  ");
    Serial.println(int(dustDensity / 0.05));
  }


  for ( int i = 0 ; i < n ; i++) {
    lcd.setCursor(10 + i, 3);
    //Serial.print("*");
    lcd.write(4);
  }


  for ( int o = 0 ; o < ( 9 - n) ; o++) {
    lcd.setCursor(10 + n + o, 3);
    lcd.print(".");
    //Serial.print("+");
  }
  //Serial.println("");
}

void requestSharp()
{
  Wire.requestFrom(2, 2);    // request 6 bytes from slave device #2

  int x;
  byte a, b;

  a = Wire.read();
  b = Wire.read();

  x = a;
  x = x << 8 | b;

  if (DEBUG_PRINT) {
    Serial.print("X ===>  ");
    Serial.println(x);
  }

  if ( x == 33333 ) {
    sleepmode = HIGH;
  }

  if ( x == 22222 ) {
    sleepmode = LOW;
  }


  if (( 1 < x ) && ( x < 1024 ) && ( x != OLD_x ))
  {
    float calcVoltage = x * (5.0 / 1024.0);
    if ( (0.17 * calcVoltage - 0.1) > 0 )
    {
      dustDensity = 0.17 * calcVoltage - 0.1;
      OLD_x = x ;
    }
  }
}

void sendI2cMsg(byte a, byte b)
{
  Wire.beginTransmission(2);
  Wire.write(a);
  Wire.write(b);
  Wire.endTransmission();
}

void senddustDensity()
{
  payload = " {\"dustDensity\":";
  payload += dustDensity;
  payload += "}";
  if ( dustDensity < 0.6 )
  {
    sendmqttMsg(payload);
  }
}

void sendmqttMsg(String payload)
{
  if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        if (client.connect((char*) clientName.c_str()))
        {
          Serial.println("Connected to MQTT broker again esp8266/arduino/s03");
          Serial.print("Topic is: ");
          Serial.println(topic);
        }
        else {
          Serial.println("MQTT connect failed");
          Serial.println("Will reset and try again...");
          ESP.restart();
        }
      }

      if (client.connected()) {
        if (DEBUG_PRINT)
        {
          Serial.print("Sending payload: ");
          Serial.println(payload);
        }

        if (client.publish(topic, (char*) payload.c_str())) {
          if (DEBUG_PRINT)
          {
            Serial.println("Publish ok");
          }
        }
        else {
          Serial.println("Publish failed");
          ESP.restart(); 
        }
      }
  } else {
      Serial.println("Could not connect to WIFI");
      ESP.restart();    
  }
}


String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5) {
      result += ':';
    }
  }
  return result;
}

void digitalClockDisplay()
{
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

void printDigitsnocolon(int digits)
{
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}


void printDigits(int digits)
{
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


//---------------

