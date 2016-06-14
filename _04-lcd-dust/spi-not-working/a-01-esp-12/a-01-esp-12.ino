// http://arduino.esp8266.com/stable/package_esp8266com_index.json 2.2.0
// 160MHz / 4M / 3M / ESP-12 / esp-lcddust
/* pins
  spi :
  5   : dht22
  4   : signal from nano
*/
#include <TimeLib.h>
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <SPI.h>
#include "PietteTech_DHT.h"

#define DHT_DEBUG_TIMING
#define REPORT_INTERVAL 5000 // in msec

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

#define DHTTYPE  DHT22        // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   5            // Digital pin for communications

#define DEBUG_PRINT 1
#define NANO_DATA_PIN 4

extern "C"
{
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

IPAddress influxdbudp = MQTT_SERVER;
IPAddress mqtt_server = MQTT_SERVER;
IPAddress time_server = MQTT_SERVER;

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
const char subtopic_6[] = "esp8266/cmd/ac";
const char subtopic_7[] = "esp8266/cmd/acset";

const char* substopic[8] = { subtopic_0, subtopic_1, subtopic_2, subtopic_3, subtopic_4, subtopic_5, subtopic_6, subtopic_7};

typedef struct
{
  uint32_t hash;
  uint32_t timenow;
  uint16_t temperaturein;
  uint16_t temperatureout;
  uint16_t humidity;
  uint16_t powerall;
  uint16_t powerac;
  uint16_t nemoweight;
  uint16_t nano_dustDensity;
  uint16_t nano_moisture;
  uint8_t doorpir;
  uint8_t hostall;
  uint8_t hosttwo;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t nano_ac_in;
  uint8_t nano_pir_in;
} data;

data data_esp;
data data_nano;
data data_mqtt;

volatile bool balm_isr;

long lastReconnectAttempt = 0;
unsigned long startMillis;
unsigned long sentMills ;

String clientName;
String payload;
String syslogPayload;
String getResetInfo;

bool resetInfosent = false;

unsigned int localPort = 2390;
const int timeZone = 9;

// globals / dht22
bool bDHTstarted = false;
int acquireresult = 0;
int acquirestatus = 0;
int _sensor_error_count = 0;
unsigned long _sensor_report_count = 0;

float temperaturein2;

/* for hash */
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length)
{
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data)
{
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

template <typename T> unsigned int SPI_writeAnything (const T& value)
{
  const byte * p = (const byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
  {
    SPI.transfer(*p++);
    //delayMicroseconds(2);
  }
  return i;
}  // end of SPI_writeAnything

template <typename T> unsigned int SPI_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
  {
    *p++ = SPI.transfer (0);
    //delayMicroseconds(2);
  }
  return i;
}  // end of SPI_readAnything

void callback(char* intopic, byte* inpayload, unsigned int length);
void ICACHE_RAM_ATTR dht_wrapper();

WiFiClient wifiClient;
WiFiUDP udp;
PubSubClient client(wifiClient);
//PubSubClient client(mqtt_server, 1883, callback, wifiClient);
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

void ICACHE_RAM_ATTR dht_wrapper()
{
  DHT.isrCallback();
}

void callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  if ( receivedtopic == "esp8266/arduino/s03" || receivedtopic == "clients/dust" )
  {
    return;
  }

  for (int i = 0; i < length; i++)
  {
    receivedpayload += (char)inpayload[i];
  }
  parseMqttMsg(receivedpayload, receivedtopic);
}

void parseMqttMsg(String receivedpayload, String receivedtopic)
{
  char json[] = "{\"Humidity\":43.90,\"Temperature\":22.00,\"DS18B20\":22.00,\"PIRSTATUS\":0,\"FreeHeap\":43552,\"acquireresult\":0,\"acquirestatus\":0,\"DHTnextSampleTime\":2121587,\"bDHTstarted\":0,\"RSSI\":-48,\"millis\":2117963}";

  receivedpayload.toCharArray(json, 450);
  StaticJsonBuffer<450> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    return;
  }

  if ( receivedtopic == substopic[0] )
  {
    if (root.containsKey("DS18B20"))
    {
      data_mqtt.temperaturein = (float)root["DS18B20"] * 10;
    }
  }
  
  if ( receivedtopic == substopic[1] )
  {
    if (root.containsKey("data1"))
    {
      data_mqtt.temperatureout = (float)root["data1"] * 10;
    }
  }
  
  if ( receivedtopic == substopic[2] )
  {
    if (root.containsKey("powerAvg"))
    {
      data_mqtt.powerall = root["powerAvg"];
    }
    if (root.containsKey("powerAC"))
    {
      data_mqtt.powerac = root["powerAC"];
    }
  }
  
  if ( receivedtopic == substopic[3] )
  {
    if (root.containsKey("WeightAvg"))
    {
      if (  root["WeightAvg"] >= 0)
      {
        data_mqtt.nemoweight = root["WeightAvg"];
      }
      else {
        data_mqtt.nemoweight = 0;
      }
    }
  }
  
  if ( receivedtopic == substopic[4] )
  {
    if (root.containsKey("DOORPIR"))
    {
      data_mqtt.doorpir = root["DOORPIR"];
    }
  }
  
  if ( receivedtopic == substopic[5] )
  {
    if (root.containsKey("unihost"))
    {
      data_mqtt.hosttwo = root["unihost"];
      //unihost = atoi(tempunihost);
    }
    if (root.containsKey("unitot"))
    {
      data_mqtt.hostall = root["unitot"];
    }
  }
  
  if ( receivedtopic == substopic[6] )
  {
    if (root.containsKey("AC"))
    {
      data_mqtt.accmd = root["AC"];
    }
  }
  
  if ( receivedtopic == substopic[7] )
  {
    if (root.containsKey("actemp"))
    {
      data_mqtt.actemp = root["actemp"];
    }
    if (root.containsKey("acflow"))
    {
      data_mqtt.acflow = root["acflow"];
    }
  }
  balm_isr = true;
}

boolean reconnect()
{
  if (client.connect((char*) clientName.c_str(), willTopic, 0, true, willMessage))
  {
    client.publish(willTopic, "1", true);
    client.loop();
    if (resetInfosent == false)
    {
      client.publish(hellotopic, (char*) getResetInfo.c_str());
      resetInfosent = true;
    }
    else
    {
      client.publish(hellotopic, "hello again 1 from lcd ");
    }

    client.loop();
    for (int i = 0; i < 8; ++i)
    {
      client.subscribe(substopic[i]);
      client.loop();

      if (DEBUG_PRINT)
      {
        Serial.print("subscribed to : ");
        Serial.print(i);
        Serial.print(" - ");
        Serial.println(substopic[i]);
      }
    }

    if (DEBUG_PRINT)
    {
      Serial.println("---> mqttconnected");
    }
  }
  else
  {
    if (DEBUG_PRINT)
    {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
  }
  return client.connected();
}

void alm_isr ()
{
  balm_isr = true;
}

void wifi_connect() 
{

#define IPSET_STATIC { 192, 168, 10, 61 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

  IPAddress ip_static = IPSET_STATIC;
  IPAddress ip_gateway = IPSET_GATEWAY;
  IPAddress ip_subnet = IPSET_SUBNET;
  IPAddress ip_dns = IPSET_DNS;
  
  wifi_set_phy_mode(PHY_MODE_11N);
  //system_phy_set_max_tpw(1);
  WiFi.mode(WIFI_STA);
  wifi_station_connect();
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));
  WiFi.hostname("esp-lcddust");

  Serial.print("WIFI connect to : ");
  Serial.println(ssid);
  
  int Attempt = 1;
  Serial.println();
  while (WiFi.status() != WL_CONNECTED) 
  {
    Serial.print(". ");
    Serial.print(Attempt);
    if ( Attempt % 15 == 0 )
    {
      Serial.println();
    }
    delay(100);
    Attempt++;
    if (Attempt == 300) 
    {
      Serial.println();
      Serial.println("-----> Could not connect to WIFI");
      Serial.flush();
      ESP.reset();
    }
  }

  Serial.println();
  Serial.print("===> WiFi connected");
  Serial.print(" ------> IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{
  system_update_cpu_freq(SYS_CPU_160MHz);
  //WiFi.mode(WIFI_OFF);
  
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }
 
  Serial.println("Serial connected");
  Serial.flush();
  
  //pinMode(NANO_DATA_PIN, INPUT);
  balm_isr = false;
  
  startMillis = sentMills = millis();

  getResetInfo = "hello from ESP8266 lcd ";
  getResetInfo += ESP.getResetInfo().substring(0, 60);

  clientName += "esp8266 - ";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += " - ";
  clientName += String(micros() & 0xff, 16);

  Serial.println(clientName);

  WiFiClient::setLocalPortStart(analogRead(A0));
  wifi_connect();

  udp.begin(localPort);
  while ( timeStatus() == timeNotSet )
  {
    setSyncProvider(getNtpTime);
    Serial.print(". ");
    delay(300);
  }
  Serial.println("ntp synced");

  // mqtt
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // spi
  SPI.begin ();
  SPI.setHwCs(true);
  //SPISettings(4000000, MSBFIRST, SPI_MODE0);

  // data for struct
  data_esp.timenow          = now();
  data_esp.temperaturein    = 0;
  data_esp.temperatureout   = 0;
  data_esp.humidity         = 0;
  data_esp.powerall         = 0;
  data_esp.powerac          = 0;
  data_esp.nemoweight       = 0;
  data_esp.doorpir          = 0;
  data_esp.hostall          = 0;
  data_esp.hosttwo          = 0;
  data_esp.accmd            = 0;
  data_esp.actemp           = 0;
  data_esp.acflow           = 0;
  data_esp.nano_dustDensity = 0;
  data_esp.nano_moisture    = 0;
  data_esp.nano_ac_in       = 0;
  data_esp.nano_pir_in      = 0;
  data_esp.hash = calc_hash(data_esp);

  data_mqtt.timenow          = now();
  data_mqtt.temperaturein    = 0;
  data_mqtt.temperatureout   = 0;
  data_mqtt.humidity         = 0;
  data_mqtt.powerall         = 0;
  data_mqtt.powerac          = 0;
  data_mqtt.nemoweight       = 0;
  data_mqtt.doorpir          = 0;
  data_mqtt.hostall          = 0;
  data_mqtt.hosttwo          = 0;
  data_mqtt.accmd            = 0;
  data_mqtt.actemp           = 0;
  data_mqtt.acflow           = 0;
  data_mqtt.nano_dustDensity = 0;
  data_mqtt.nano_moisture    = 0;
  data_mqtt.nano_ac_in       = 0;
  data_mqtt.nano_pir_in      = 0;
  data_mqtt.hash = calc_hash(data_mqtt);

  temperaturein2 = 0;

  // dht22
  acquireresult = DHT.acquireAndWait(1000);
  if (acquireresult != 0)
  {
    _sensor_error_count++;
  }

  if ( acquireresult == 0 )
  {
    temperaturein2 = DHT.getCelsius();
    data_esp.humidity = DHT.getHumidity() * 10;
  }

  if (DEBUG_PRINT)
  {
    Serial.println("------------------> unit started");
  }

  //attachInterrupt(NANO_DATA_PIN, alm_isr, RISING);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      if (DEBUG_PRINT)
      {
        Serial.print("failed, rc= ");
        Serial.println(client.state());
      }
      unsigned long now = millis();
      if (now - lastReconnectAttempt > 100)
      {
        lastReconnectAttempt = now;
        if (reconnect())
        {
          lastReconnectAttempt = 0;
        }
      }
    }
    else
    {
      if (bDHTstarted)
      {
        acquirestatus = DHT.acquiring();
        if (!acquirestatus)
        {
          acquireresult = DHT.getStatus();
#if defined(DHT_DEBUG_TIMING)
          printEdgeTiming(&DHT);
#endif
          if ( acquireresult == 0 )
          {
            temperaturein2 = DHT.getCelsius() * 10;
            data_mqtt.humidity = DHT.getHumidity() * 10;
          }
          bDHTstarted = false;
        }
      }

      if ((millis() - sentMills) > REPORT_INTERVAL )
      {
        payload = "{\"dustDensity\":";
        payload += (data_esp.nano_dustDensity * 0.01);
        payload += ",\"moisture\":";
        payload += data_esp.nano_moisture;
        payload += ",\"Humidity\":";
        payload += (data_mqtt.humidity * 0.1);
        payload += ",\"Temperature\":";
        payload += (temperaturein2 * 0.1);
        payload += ",\"acquireresult\":";
        payload += acquireresult;
        payload += ",\"acquirestatus\":";
        payload += acquirestatus;
        payload += ",\"bDHTstarted\":";
        payload += bDHTstarted;
        payload += ",\"FreeHeap\":";
        payload += ESP.getFreeHeap();
        payload += ",\"RSSI\":";
        payload += WiFi.RSSI();
        payload += ",\"millis\":";
        payload += millis();
        payload += "}";

        sendmqttMsg(payload);

        sentMills = millis();

        if (acquirestatus == 1)
        {
          DHT.reset();
        }

        if (!bDHTstarted)
        {
          DHT.acquire();
          bDHTstarted = true;
        }
      }

      if (balm_isr)
      {
        Serial.print("alarm on : ");
        Serial.println(now());

        data_esp.timenow = now();
        data_esp.temperaturein    = (data_mqtt.temperaturein + temperaturein2 ) / 2;
        data_esp.temperatureout   = data_mqtt.temperatureout;
        data_esp.humidity         = data_mqtt.humidity;
        data_esp.powerall         = data_mqtt.powerall;
        data_esp.powerac          = data_mqtt.powerac;
        data_esp.nemoweight       = data_mqtt.nemoweight;
        data_esp.doorpir          = data_mqtt.doorpir;
        data_esp.hostall          = data_mqtt.hostall;
        data_esp.hosttwo          = data_mqtt.hosttwo;
        data_esp.accmd            = data_mqtt.accmd;
        data_esp.actemp           = data_mqtt.actemp;
        data_esp.acflow           = data_mqtt.acflow;
        data_esp.hash             = calc_hash(data_esp);

        SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
        SPI_writeAnything(data_esp);
        SPI.endTransaction();

        /*
        SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
        SPI_readAnything(data_nano);
        SPI.endTransaction();
        */

        if (data_nano.hash == calc_hash(data_nano))
        {
          data_esp.nano_dustDensity = data_nano.nano_dustDensity;
          data_esp.nano_moisture    = data_nano.nano_moisture;
        }
        balm_isr = false;
      }
      client.loop();
    }
  }
}

void sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(influxdbudp, 514);
  udp.write("esp-lcddust: ");
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

void printEdgeTiming(class PietteTech_DHT *_d)
{
  byte n;
#if defined(DHT_DEBUG_TIMING)
  volatile uint8_t *_e = &_d->_edges[0];
#endif
  int result = _d->getStatus();
  if (result != 0)
  {
    _sensor_error_count++;
  }

  _sensor_report_count++;

  String udppayload = "edges2,device=esp-12-N1,debug=on,DHTLIB_ONE_TIMING=110 ";
  for (n = 0; n < 41; n++)
  {
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

void sendmqttMsg(String payloadtosend)
{
  unsigned int msg_length = payloadtosend.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payloadtosend.c_str(), msg_length);

  if ( client.publish(topic, p, msg_length, 1))
  {
    if (DEBUG_PRINT) {
      Serial.print(payloadtosend);
      Serial.println(" : Publish ok");
    }
    free(p);
  }
  else
  {
    if (DEBUG_PRINT)
    {
      Serial.print(payloadtosend);
      Serial.println(" : Publish fail");      
    }
    free(p);
  }
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
    {
      result += ':';
    }
  }
  return result;
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(time_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
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
void sendNTPpacket(IPAddress & address)
{
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
