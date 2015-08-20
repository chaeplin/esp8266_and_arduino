/*
*/
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

// mqtt
char* topic = "esp8266/arduino/s02";
char* subtopic = "esp8266/cmd/light";
char* hellotopic = "HELLO";
IPAddress server(192, 168, 10, 10);

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

void callback(const MQTT::Publish& pub) {
  
  Serial.print(" => ");
  Serial.print(pub.topic());
  Serial.print(" => ");
  Serial.print(pub.payload_string());
  
  String subPayload = pub.payload_string() ;

  if ( subPayload == "{\"LIGHT\":1}") 
  {
       relaystatus = 1 ;
    
  } else if ( subPayload == "{\"LIGHT\":0}") 
  {
       relaystatus = 0 ;
  }
  changelight() ;
  Serial.println("");
  Serial.print(" => relaystatus => ");
  Serial.println(relaystatus);
}

WiFiClient wifiClient;
PubSubClient client(wifiClient, server);
WiFiUDP udp;


void setup() 
{
  Serial.begin(38400);
  Serial.println("DHTxx test!");
  delay(20);

  client.set_callback(callback);
  
  startMills = millis();

  pinMode(pir,INPUT);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(TOPBUTTONPIN, INPUT_PULLUP);

  digitalWrite(RELAYPIN, relaystatus);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password); 

  #ifdef __IS_MY_HOME
  WiFi.config(IPAddress(192, 168, 10, 11), IPAddress(192, 168, 10, 1), IPAddress(255, 255, 255, 0));
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

  attachInterrupt(13, motion_detection, RISING);
  attachInterrupt(5, run_lightcmd, CHANGE); 

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

    if (client.publish(hellotopic, "hello from esp8266/arduino/s02")) {
      Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
    }

    if (client.subscribe(subtopic)) {
      Serial.println("Subscribe ok");
    } else {
      Serial.println("Subscribe failed");
    }

  }
  else {
    Serial.println("MQTT connect failed");
    Serial.println("Will reset and try again...");
    abort();
  }

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

  client.loop();
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
      Serial.println("Connected to MQTT broker again TEMP");
      Serial.print("Topic is: ");
      Serial.println(topictosend);
    }
    else {
      Serial.println("MQTT connect failed");
      Serial.println("Will reset and try again...");
      abort();
    }
  }

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.println(payload);

    if (
      client.publish(MQTT::Publish(topictosend, (char*) payload.c_str())
                .set_retain()
               )
      ) {
      //Serial.println("Publish ok");
    }
    else {
      Serial.println("Publish failed");
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

// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    // zero pad the address if necessary
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}

// function to print the temperature for a device
void printTemperature(DeviceAddress deviceAddress)
{
  float tempC = sensors.getTempC(deviceAddress);
  Serial.print("Temp C: ");
  Serial.print(tempC);
  Serial.print(" Temp F: ");
  Serial.print(DallasTemperature::toFahrenheit(tempC));
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
  Serial.print("Resolution: ");
  Serial.print(sensors.getResolution(deviceAddress));
  Serial.println();    
}

// main function to print information about a device
void printData(DeviceAddress deviceAddress)
{
  Serial.print("Device Address: ");
  printAddress(deviceAddress);
  Serial.print(" ");
  printTemperature(deviceAddress);
  Serial.println();
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
