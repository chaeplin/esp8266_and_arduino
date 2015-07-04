#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <Time.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>
#include <RtcDS3231.h>

RtcDS3231 Rtc;

#define DEBUG_PRINT 0

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

char* topic = "esp8266/arduino/s03";
char* subtopic = "#";
char* hellotopic = "HELLO";

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
float NW ;
int PIR  ;
float dustDensity ;

float OLD_H  ;
float OLD_T1 ;
float OLD_T2 ;
float OLD_OT ;
float OLD_PW ;
float OLD_NW ;
int OLD_PIR  ;
float OLD_dustDensity ;

int OLD_x ;

byte termometru[8] = //icon for termometer
{
  B00100,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110
};

byte picatura[8] = //icon for water droplet
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

/*
byte dustDensityicon[8] = //icon for dustDensity droplet
{
  B00000,
  B01110,
  B01010,
  B01110,
  B01110,
  B01010,
  B01110,
  B00000,
};
*/

byte dustDensityicon[8] = //icon for dustDensity droplet
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

byte dustDensityfill[8] = //icon for dustDensity droplet
{
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B00000,
};


byte pirfill[8] = //icon for dustDensity droplet
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

byte powericon[8] = //icon for dustDensity droplet
{
  B11000,
  B00110,
  B00011,
  B01100,
  B11000,
  B00110,
  B00011,
  B01100,
};



PubSubClient client(wifiClient, server);

void callback(const MQTT::Publish& pub) {
  if (DEBUG_PRINT) {
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

  float TEMP_H   = root["Humidity"];
  float TEMP_T1  = root["Temperature"];
  float TEMP_T2  = root["DS18B20"];
  float TEMP_OT  = root["OUTSIDE"];
  float TEMP_PW  = root["powerAvg"];
  float TEMP_NW  = root["NemoWeightAvg"];
  int TEMP_PIR   = root["DOORPIR"];

  if ( TEMP_H > 0 )
  {
    H   = root["Humidity"];
  }

  if ( TEMP_T1 > 0 )
  {
    T1  = root["Temperature"];
  }

  if (TEMP_T2 > 0 )
  {
    T2  = root["DS18B20"];
  }

  if ( TEMP_OT > 0 )
  {
    OT  = root["OUTSIDE"];
  }

  if ( TEMP_PW > 0 )
  {
    PW  = root["powerAvg"];
  }

  if ( TEMP_NW > 0 )
  {
    NW  = root["NemoWeightAvg"];
  }

  if ( receivedtopic == "raspberrypi/doorpir" )
  {
    PIR = root["DOORPIR"];
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

  client.set_callback(callback);

  WiFi.mode(WIFI_STA);

#ifdef __IS_MY_HOME
  WiFi.begin(ssid, password, channel, bssid);
  WiFi.config(IPAddress(192, 168, 10, 12), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
#else
  WiFi.begin(ssid, password);
#endif

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
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

  if (client.connect((char*) clientName.c_str())) {
    Serial.println("Connected to MQTT broker");
    Serial.print("Topic is: ");
    Serial.println(topic);

    if (client.publish(hellotopic, "hello from ESP8266 s03")) {
      Serial.println("Publish ok");
    } else {
      Serial.println("Publish failed");
    }

    if (client.subscribe(subtopic)) {
      Serial.println("Subscribe ok");
    } else {
      Serial.println("Subscribe failed");
    }

  } else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }

  lcd.init();                      // initialize the lcd
  lcd.backlight();
  lcd.clear();
  lcd.createChar(1, termometru);
  lcd.createChar(2, picatura);
  lcd.createChar(3, dustDensityicon);
  lcd.createChar(4, dustDensityfill);
  lcd.createChar(5, pirfill);
  lcd.createChar(6, powericon);

  lcd.setCursor(0, 1);
  lcd.write(1);

  lcd.setCursor(0, 2);
  lcd.write(2);

  lcd.setCursor(0, 3);
  lcd.write(3);

  lcd.setCursor(8, 2);
  lcd.write(6);


  H  = 0 ;
  T1 = 0 ;
  T2 = 0 ;
  OT = 0 ;
  PW = 0 ;
  NW = 0 ;
  PIR = 0 ;
  dustDensity = 0;

  OLD_H  = 0 ;
  OLD_T1 = 0 ;
  OLD_T2 = 0 ;
  OLD_OT = 0 ;
  OLD_PW = 0 ;
  OLD_NW = 0 ;
  OLD_PIR = 0 ;
  OLD_dustDensity = 0 ;

  OLD_x = 0 ;

}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      requestSharp();
      checkDisplayValue();
    }
  }

  client.loop();

}

void checkDisplayValue() {
  if ( PW != OLD_PW )
  {
    displaypowerAvg();
    OLD_PW = PW;
  }

  if ( NW != OLD_NW )
  {
    displayNemoWeightAvg();
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
    Serial.println(NW);
  }

}

void displaypowerAvg()
{
  lcd.setCursor(10, 2);
  if ( PW > 999 ) {
    lcd.print(PW, 0);
    lcd.print("W");
  } else {
    lcd.print(" ");
    lcd.print(PW, 0);
    lcd.print("W");
  }

}

void displayNemoWeightAvg()
{

}

void displayPIR()
{
  if ( PIR == 1)
  {
    lcd.setCursor(19, 0);
    lcd.write(5);
    lcd.setCursor(19, 1);
    lcd.write(5);
    lcd.setCursor(19, 2);
    lcd.write(5);
    lcd.setCursor(19, 3);
    lcd.write(5);
  } else {
    lcd.setCursor(19, 0);
    lcd.print(" ");
    lcd.setCursor(19, 1);
    lcd.print(" ");
    lcd.setCursor(19, 2);
    lcd.print(" ");
    lcd.setCursor(19, 3);
    lcd.print(" ");
  }
}

void displayTemperature()
{

  lcd.setCursor(2, 1);
  lcd.print((T1 + T2) / 2, 1);
  lcd.print((char)223); //degree sign

  float tempdiff = OT - ((T1 + T2) / 2) ;

  if ( OT > 0 ) {
    lcd.print(" ");
    lcd.print(OT, 1);
    lcd.print((char)223); //degree sign
    lcd.print(" ");

    if ( tempdiff >= 0 ) {
      lcd.print("+");
    }
    lcd.print(OT - ((T1 + T2) / 2), 1);
    lcd.print((char)223); //degree sign
  }

  lcd.setCursor(2, 2);
  lcd.print(H, 1);
  lcd.print("%");
}

void displaydustDensity()
{
  int n = int(dustDensity / 0.05) ;

  if (DEBUG_PRINT) {
    Serial.print("===> dustDensity ");
    Serial.print(dustDensity);
    Serial.print(" ===>  ");
    Serial.println(int(dustDensity / 0.05));
  }

  for ( int i = 0 ; i <= n ; i++) {
    lcd.setCursor(2 + i, 3);
    lcd.write(4);
  }

  for ( int o = 0 ; o <= ( 10 - n) ; o++) {
    lcd.print(" ");
  }
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

  if (( x < 1024 ) && (x != OLD_x )) {
    float calcVoltage = x * (5.0 / 1024.0);
    dustDensity = 0.17 * calcVoltage - 0.1;
    OLD_x = x ;
    // //    0 ~ 0.5
  }

}


void senddustDensity()
{
  payload = " {\"dustDensity\":";
  payload += dustDensity;
  payload += "}";

  sendmqttMsg(payload);
}

void sendmqttMsg(String payload)
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("Connected to MQTT broker again esp8266/arduino/s03");
      Serial.print("Topic is: ");
      Serial.println(topic);
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    if (DEBUG_PRINT) {
      Serial.print("Sending payload: ");
      Serial.println(payload);
    }

    if (client.publish(topic, (char*) payload.c_str())) {
      if (DEBUG_PRINT) {
        Serial.println("Publish ok");
      }
    }
    else {
      Serial.println("Publish failed");
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
  if (digits < 10)
    lcd.print('0');
  lcd.print(digits);
}


void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  lcd.print(":");
  if (digits < 10)
    lcd.print('0');
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
void sendNTPpacket(IPAddress &address)
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

