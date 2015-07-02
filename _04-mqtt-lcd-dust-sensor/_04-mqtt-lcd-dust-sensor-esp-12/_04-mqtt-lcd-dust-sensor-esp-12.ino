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

// wifi
#ifdef __IS_MY_HOME
  #include "/usr/local/src/ap_setting.h"
#else
  #include "ap_setting.h"
#endif

char* topic = "esp8266/arduino/s03";
char* subtopic = "esp8266/#";
char* hellotopic = "HELLO";

LiquidCrystal_I2C lcd(0x27, 20, 4);

IPAddress server(192, 168, 10, 10);

unsigned int localPort = 2390;      // local port to listen for UDP packets
IPAddress timeServer(192, 168, 10, 10); // time.nist.gov NTP server
const int timeZone = 9;

String clientName;
WiFiClient wifiClient;
WiFiUDP udp;

float H  ;
float T1 ;
float T2 ;
int pir  ;
float O  ;

int inuse = 0 ;

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


PubSubClient client(wifiClient, server);

void callback(const MQTT::Publish& pub) {
  Serial.print(pub.topic());
  Serial.print(" => ");
  Serial.println(pub.payload_string());
  inuse = 1;
  lcd_display(pub.payload_string());
}

void lcd_display(String payload) {
  StaticJsonBuffer<200> jsonBuffer;

  char json[] =
    "{\"Humidity\":38.10,\"Temperature\":27.70,\"DS18B20\":28.31,\"PIRSTATUS\":1}";

  payload.toCharArray(json, 200);

  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return;
  }
  
  float Of   = root["OUTSIDE"];
  float Hf   = root["Humidity"];
  float T1f  = root["Temperature"];
  float T2f  = root["DS18B20"];
  int pirf   = root["PIRSTATUS"];

  if ( Of > 0 ) {
     O = Of ;
  }
  if ( Hf > 0 ) {
    H   = Hf;
    T1  = T1f ;
    T2  = T2f;
    pir = pirf ;
  }

  Serial.print(H);
  Serial.print("-");
  Serial.print(T1);
  Serial.print("-");
  Serial.print(T2);
  Serial.print("-");
  Serial.print(pir);
  Serial.print("-");
  Serial.print(O);
  Serial.println("");


  lcd.setCursor(2, 1);
  lcd.print((T1 + T2) / 2);
  lcd.print((char)223); //degree sign

  if ( O > 0 ) {
      lcd.setCursor(2, 2);
      lcd.print("      ");
      lcd.setCursor(2, 2);
      lcd.print(O);
      lcd.print((char)223); //degree sign
  }
  lcd.setCursor(2, 3);
  lcd.print(H);
  lcd.print("%");

  inuse = 0;

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


  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
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
  lcd.createChar(1,termometru);
  lcd.createChar(2,picatura);

  lcd.setCursor(0, 1);
  lcd.write(1);
  lcd.setCursor(0, 2);
  lcd.write(1);
  lcd.setCursor(0, 3);
  lcd.write(2);

}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop() {

  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      requestSharp();
    }
  }

  client.loop();

}

void requestSharp() {
  if ( inuse == 0 ) {
     Wire.requestFrom(2, 2);    // request 6 bytes from slave device #2
     
     int x;
     byte a, b;

     a = Wire.read();
     b = Wire.read();

     x = a;
     x = x << 8 | b;     
     
     float calcVoltage = x * (5.0 / 1024.0);
     float dustDensity = 0.17 * calcVoltage - 0.1;

     Serial.print("Raw Signal Value (0-1023): ");
     Serial.print(x);
  
      Serial.print(" - Voltage: ");
      Serial.print(calcVoltage);
  
      Serial.print(" - Dust Density: ");
      Serial.println(dustDensity); // unit: mg/m3

       lcd.setCursor(9, 1);
       lcd.print("*    ");
       lcd.setCursor(10, 1);
       lcd.print(x);
       
       lcd.setCursor(9, 2);
       lcd.print("*");
       lcd.print(calcVoltage);
       lcd.print(" Volt");
       
       lcd.setCursor(9, 3);
       lcd.print("*");
       lcd.print(dustDensity);
       lcd.print(" mg/m3");
     
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
  if (digits < 10)
    lcd.print('0');
  lcd.print(digits);
}


void printDigits(int digits) {
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

