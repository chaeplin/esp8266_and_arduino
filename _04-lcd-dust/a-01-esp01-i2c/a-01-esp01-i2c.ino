// http://arduino.esp8266.com/stable/package_esp8266com_index.json 2.2.0
// 160MHz / 1M / 64K / ESP-01 / esp-lcddust
/* pins
  esp-01
  i2c(0/2)
  sqwv  : 1/tx
  dht22 : 3/rx
*/
#include <TimeLib.h>
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "PietteTech_DHT.h"
#include <RtcDS3231.h>

#define SLAVE_ADDRESS 8

#define DHT_DEBUG_TIMING
#define REPORT_INTERVAL 5000 // in msec

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

#define DHTTYPE  DHT22        // Sensor type DHT11/21/22/AM2301/AM2302
#define DHTPIN   3            // Digital pin for communications

#define DEBUG_PRINT 1
#define SQWV_PIN 1

extern "C"
{
#include "user_interface.h"
}

#include "/usr/local/src/ap_setting.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
const char* otapassword = OTA_PASSWORD;

IPAddress mqtt_server = MQTT_SERVER;

char* topic = "esp8266/arduino/s03";
char* hellotopic = "HELLO";
char* willTopic = "clients/dust";
char* willMessage = "0";
char* reporttopic = "esp8266/report/s03";

const char subtopic_0[] = "esp8266/arduino/s02";
const char subtopic_1[] = "radio/test/2";
const char subtopic_2[] = "esp8266/arduino/s07";
const char subtopic_3[] = "esp8266/arduino/s06";
const char subtopic_4[] = "raspberrypi/doorpir";
const char subtopic_5[] = "home/check/checkhwmny";
const char subtopic_6[] = "esp8266/cmd/ac";
const char subtopic_7[] = "esp8266/cmd/acset";
const char subtopic_8[] = "esp8266/check";

const char* substopic[9] =
{
  subtopic_0,
  subtopic_1,
  subtopic_2,
  subtopic_3,
  subtopic_4,
  subtopic_5,
  subtopic_6,
  subtopic_7,
  subtopic_8
};

volatile struct
{
  uint32_t hash;
  float dustDensity;
  uint16_t moisture;
  uint16_t irrecvd;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t acauto;
} data_nano;

volatile struct
{
  uint32_t hash;
  float tempeinside;
  float tempeoutside;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t acauto;
} data_esp;

typedef struct
{
  float dustDensity;
  float tempinside1;
  float tempinside2;
  float tempoutside;
  float humidity;
  uint16_t powerall;
  uint16_t powerac;
  uint16_t nemoweight;
  uint16_t doorpir;
  uint8_t hostall;
  uint8_t hosttwo;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t acauto;
} data;

data data_mqtt, data_curr;

volatile bool balm_isr = false;
volatile bool msgcallback = false;
long lastReconnectAttempt = 0;
unsigned long sentMills;
bool resetInfosent = false;
unsigned int localPort = 2390;
const int timeZone = 9;

String clientName;
String payload;
String syslogPayload;
String getResetInfo;

// globals / dht22
bool bDHTstarted = false;
int acquireresult = 0;
int acquirestatus = 0;
int _sensor_error_count = 0;
unsigned long _sensor_report_count = 0;

// https://omerk.github.io/lcdchargen/
const char termometru[8]      PROGMEM = { B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110 };
const char picatura[8]        PROGMEM = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110 };
const char dustDensityicon[8] PROGMEM = { B11111, B11111, B11011, B10001, B10001, B11011, B11111, B11111 };
const char dustDensityfill[8] PROGMEM = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };
const char pirfill[8]         PROGMEM = { B00111, B00111, B00111, B00111, B00111, B00111, B00111, B00111 };
const char powericon[8]       PROGMEM = { B11111, B11011, B10001, B11011, B11111, B11000, B11000, B11000 };
const char nemoicon[8]        PROGMEM = { B11011, B11011, B00100, B11111, B10101, B11111, B01010, B11011 };
const char callbackicon[8]    PROGMEM = { B11111, B11111, B11111, B11111, B00000, B00000, B00000, B00000 };

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

/* for i2c */
template <typename T> unsigned int I2C_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = Wire.read();
  return i;
}

template <typename T> unsigned int I2C_writeAnything (const T& value)
{
  Wire.write((byte *) &value, sizeof (value));
  return sizeof (value);
}

void callback(char* intopic, byte* inpayload, unsigned int length);
void ICACHE_RAM_ATTR dht_wrapper();

WiFiClient wifiClient;
WiFiUDP udp;
RtcDS3231 Rtc;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
LiquidCrystal_I2C lcd(0x27, 20, 4);
PietteTech_DHT DHT(DHTPIN, DHTTYPE, dht_wrapper);

void ICACHE_RAM_ATTR dht_wrapper()
{
  DHT.isrCallback();
}

void ICACHE_RAM_ATTR sendUdpSyslog(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(mqtt_server, 514);
  udp.write("esp-lcddust: ");
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR sendUdpmsg(String msgtosend)
{
  unsigned int msg_length = msgtosend.length();
  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) msgtosend.c_str(), msg_length);

  udp.beginPacket(mqtt_server, 8089);
  udp.write(p, msg_length);
  udp.endPacket();
  free(p);
}

void ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payload)
{
  unsigned int msg_length = payload.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payload.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, 1))
  {
    /*
      syslogPayload = topictosend;
      syslogPayload += " - ";
      syslogPayload += payload;
      syslogPayload += " : Publish ok";
      sendUdpSyslog(syslogPayload);
    */
    free(p);
  }
  else
  {
    syslogPayload = topictosend;
    syslogPayload += " - ";
    syslogPayload += payload;
    syslogPayload += " : Publish fail";
    sendUdpSyslog(syslogPayload);
    free(p);
  }
  client.loop();
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
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

  if (DEBUG_PRINT) {
    syslogPayload = "mqtt ====> ";
    syslogPayload += intopic;
    syslogPayload += " ====> ";
    syslogPayload += receivedpayload;
    sendUdpSyslog(syslogPayload);
  }

  if ( receivedpayload == "{\"CHECKING\":\"1\"}")
  {
    String check_payload = "ac status: ";
    if (data_curr.accmd == 1)
    {
      check_payload += "off";
    }
    else
    {
      check_payload += "on";
    }
    check_payload += "\r\n";
    check_payload += "temp inside: ";
    check_payload += ((data_mqtt.tempinside1 + data_mqtt.tempinside2) / 2);
    check_payload += "\r\n";
    check_payload += "temp outside: ";
    check_payload += data_mqtt.tempoutside;
    check_payload += "\r\n";
    check_payload += "humidity: ";
    check_payload += data_mqtt.humidity;
    check_payload += "\r\n";
    check_payload += "dustDensity: ";
    check_payload += data_curr.dustDensity;
    check_payload += "\r\n";
    check_payload += "soil moisture: ";
    check_payload += data_nano.moisture;
    check_payload += "\r\n";
    check_payload += "host all/2: ";
    check_payload += data_mqtt.hostall;
    check_payload += "/";
    check_payload += data_mqtt.hosttwo;

    sendmqttMsg(reporttopic, check_payload);
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
      data_mqtt.tempinside1 = root["DS18B20"];
    }
  }

  if ( receivedtopic == substopic[1] )
  {
    if (root.containsKey("data1"))
    {
      data_mqtt.tempoutside = root["data1"];
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

      if ( data_mqtt.powerac == 1)
      {
        data_curr.accmd = 2;
      }
      else
      {
        data_curr.accmd = 1;
      }
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
      switch (data_mqtt.accmd)
      {
        case 0:
          data_curr.accmd = data_esp.accmd = 2;
          break;

        case 1:
          data_curr.accmd = data_esp.accmd = 2;
          break;

        case 2:
          data_curr.accmd = data_esp.accmd = 1;
          break;

        case 3:
          data_curr.accmd = data_esp.accmd = 2;
          break;

        case 4:
          data_curr.accmd = data_esp.accmd = 1;
          break;

        case 5:
          if (data_curr.accmd == 1)
          {
            data_curr.accmd = data_esp.accmd = 2;
          }
          else
          {
            data_curr.accmd = data_esp.accmd = 1;
          }
          break;

        default:
          data_curr.accmd = data_esp.accmd = 1;
          break;
      }

      String ac_payload = "ac status : ";
      if (data_curr.accmd == 1)
      {
        ac_payload += "off";
      }
      else
      {
        ac_payload += "on";
      }

      sendmqttMsg(reporttopic, ac_payload);

      if (DEBUG_PRINT) {
        syslogPayload = "accmd ====> ";
        syslogPayload += data_esp.accmd;
        sendUdpSyslog(syslogPayload);
      }
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
    if (root.containsKey("acauto"))
    {
      data_mqtt.acauto = root["acauto"];
    }
  }
  msgcallback = !msgcallback;
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
    for (int i = 0; i < 9; ++i)
    {
      client.subscribe(substopic[i]);
      client.loop();
      yield();

      if (DEBUG_PRINT)
      {
        syslogPayload  = "subscribed to : ";
        syslogPayload += i;
        syslogPayload += " - ";
        syslogPayload += substopic[i];
        sendUdpSyslog(syslogPayload);
        yield();
      }
    }

    if (DEBUG_PRINT)
    {
      sendUdpSyslog("---> mqttconnected");
    }
  }
  else
  {
    if (DEBUG_PRINT)
    {
      syslogPayload = "failed, rc=";
      syslogPayload += client.state();
      sendUdpSyslog(syslogPayload);
    }
  }
  return client.connected();
}

void alm_isr ()
{
  balm_isr = true;
}

void lcd_redraw()
{
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

  lcd.setCursor(6, 1);
  lcd.print((char)223);

  lcd.setCursor(12, 1);
  lcd.print((char)223);

  lcd.setCursor(6, 2);
  lcd.print("%");
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

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("conn to: ");
  lcd.print(ssid);

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);

    int n = (Attempt % 20);
    lcd.setCursor(n, 1);
    lcd.print("*");
    if (n < 19)
    {
      lcd.setCursor((n + 1), 1);
      lcd.print(" ");
    }
    Attempt++;
    if (Attempt == 300)
    {
      ESP.restart();
    }
  }
  lcd.setCursor(0, 2);
  lcd.print("ip: ");
  lcd.print(WiFi.localIP());
  delay(1000);

}

time_t requestSync()
{
  return 0;
}

time_t requestRtc()
{
  RtcDateTime Epoch32Time = Rtc.GetDateTime();
  return (Epoch32Time + 946684800);
}

void setup()
{
  system_update_cpu_freq(SYS_CPU_160MHz);
  Serial.swap();

  pinMode(SQWV_PIN, INPUT_PULLUP);

  data_esp.tempeinside   = 0;
  data_esp.tempeoutside  = 0;
  data_esp.accmd         = 0;
  data_esp.actemp        = 27;
  data_esp.acflow        = 0;
  data_esp.acauto        = 0;
  data_esp.hash          = calc_hash(data_esp);

  data_mqtt.tempinside1  = 0;
  data_mqtt.tempinside2  = 0;
  data_mqtt.tempoutside  = 0;
  data_mqtt.humidity     = 0;
  data_mqtt.powerall     = 0;
  data_mqtt.powerac      = 0;
  data_mqtt.nemoweight   = 0;
  data_mqtt.doorpir      = 0;
  data_mqtt.hostall      = 0;
  data_mqtt.hosttwo      = 0;
  data_mqtt.accmd        = 0;
  data_mqtt.actemp       = 27;
  data_mqtt.acflow       = 0;
  data_mqtt.acauto       = 0;

  data_curr.tempinside1  = 0;
  data_curr.tempinside2  = 0;
  data_curr.tempoutside  = 0;
  data_curr.humidity     = 0;
  data_curr.powerall     = 0;
  data_curr.powerac      = 0;
  data_curr.nemoweight   = 0;
  data_curr.doorpir      = 0;
  data_curr.hostall      = 0;
  data_curr.hosttwo      = 0;
  data_curr.accmd        = 0;
  data_curr.actemp       = 27;
  data_curr.acflow       = 0;
  data_curr.acauto       = 0;

  Wire.begin(0, 2);
  //twi_setClock(200000);
  //delay(100);

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
  lcd.createChar(8, callbackicon);

  for ( int i = 1 ; i < 19 ; i++)
  {
    lcd.setCursor(i, 0);
    lcd.write(4);
    lcd.setCursor(i, 3);
    lcd.write(4);
  }

  for ( int i = 0 ; i < 4 ; i++)
  {
    lcd.setCursor(0, i);
    lcd.write(4);
    lcd.setCursor(19, i);
    lcd.write(4);
  }

  lcd.setCursor(4, 1);
  lcd.print("[Clock mode]");

  delay(1000);
  lcd.clear();

  getResetInfo = "hello from ESP8266 lcddust ";
  getResetInfo += ESP.getResetInfo().substring(0, 80);

  clientName += "esp8266 - ";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += " - ";
  clientName += String(micros() & 0xff, 16);

  WiFiClient::setLocalPortStart(analogRead(A0));
  wifi_connect();

  udp.begin(localPort);
  setSyncProvider(requestSync);

  int Attempt = 0;
  while ( timeStatus() == timeNotSet )
  {
    setSyncProvider(getNtpTime);
    Attempt++;
    if (Attempt > 3)
    {
      break;
    }
    yield();
  }

  if (timeStatus() == timeSet)
  {
    lcd.setCursor(0, 3);
    lcd.print("ntp synced");

    Rtc.SetDateTime(now() - 946684800);

    if (!Rtc.GetIsRunning())
    {
      Rtc.SetIsRunning(true);
    }
  }

  setSyncProvider(requestRtc);
  setSyncInterval(60);

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePinClockFrequency(DS3231SquareWaveClock_1Hz);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeClock);

  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-lcddust");
  ArduinoOTA.setPassword(otapassword);
  ArduinoOTA.onStart([]()
  {
    sendUdpSyslog("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]()
  {
    sendUdpSyslog("ArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    syslogPayload = "Progress: ";
    syslogPayload += (progress / (total / 100));
    sendUdpSyslog(syslogPayload);
  });
  ArduinoOTA.onError([](ota_error_t error)
  {
    //ESP.restart();
    if (error == OTA_AUTH_ERROR) abort();
    else if (error == OTA_BEGIN_ERROR) abort();
    else if (error == OTA_CONNECT_ERROR) abort();
    else if (error == OTA_RECEIVE_ERROR) abort();
    else if (error == OTA_END_ERROR) abort();
  });

  ArduinoOTA.begin();

  acquireresult = DHT.acquireAndWait(1000);
  if (acquireresult != 0)
  {
    _sensor_error_count++;
  }

  if ( acquireresult == 0 )
  {
    data_mqtt.tempinside2 = DHT.getCelsius();
    data_mqtt.humidity    = DHT.getHumidity();
  }

  sentMills = millis();
  lcd.clear();
  lcd_redraw();
  attachInterrupt(SQWV_PIN, alm_isr, FALLING);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      if (DEBUG_PRINT)
      {
        /*
          Serial.print("failed, rc= ");
          Serial.println(client.state());
        */
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
      if (timeStatus() != timeNotSet)
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
              data_mqtt.tempinside2 = DHT.getCelsius();
              data_mqtt.humidity    = DHT.getHumidity();
            }
            bDHTstarted = false;
          }
        }

        if (
          data_curr.tempinside1 != data_mqtt.tempinside1 ||
          data_curr.tempinside2 != data_mqtt.tempinside2 ||
          data_curr.tempoutside != data_mqtt.tempoutside ||
          data_curr.humidity    != data_mqtt.humidity
        )
        {
          displayTemperature();
          data_curr.tempinside1 = data_mqtt.tempinside1;
          data_curr.tempinside2 = data_mqtt.tempinside2;
          data_curr.tempoutside = data_mqtt.tempoutside;
          data_curr.humidity    = data_mqtt.humidity;
        }

        if ( data_curr.powerall != data_mqtt.powerall )
        {
          displaypowerAvg(data_mqtt.powerall);
          data_curr.powerall = data_mqtt.powerall;
        }

        if ( data_curr.nemoweight != data_mqtt.nemoweight )
        {
          displayNemoWeight(data_mqtt.nemoweight);
          data_curr.nemoweight = data_mqtt.nemoweight;
        }

        if ( data_curr.doorpir != data_mqtt.doorpir )
        {
          displayPIR(data_mqtt.doorpir);
          data_curr.doorpir = data_mqtt.doorpir;
        }

        if (
          data_curr.hostall != data_mqtt.hostall ||
          data_curr.hosttwo != data_mqtt.hosttwo
        )
        {
          displayHost(data_mqtt.hosttwo, data_mqtt.hostall);
          data_curr.hostall = data_mqtt.hostall;
          data_curr.hosttwo = data_mqtt.hosttwo;
        }

        if ( data_curr.dustDensity != data_mqtt.dustDensity )
        {
          displaydustDensity(data_nano.dustDensity);
          data_curr.dustDensity = data_mqtt.dustDensity;
        }

        if (balm_isr)
        {
          digitalClockDisplay();

          if (!Rtc.IsDateTimeValid())
          {
            lcd.setCursor(18, 0);
            lcd.print("*");
          }
          else
          {
            lcd.setCursor(18, 0);
            lcd.print(" ");
          }

          if (msgcallback)
          {
            lcd.setCursor(19, 0);
            lcd.write(8);
          }
          else
          {
            lcd.setCursor(19, 0);
            lcd.print(" ");
          }

          data_esp.actemp  = data_mqtt.actemp;
          data_esp.acflow  = data_mqtt.acflow;
          data_esp.acauto  = data_mqtt.acauto;
          data_esp.hash = calc_hash(data_esp);

          Wire.beginTransmission(SLAVE_ADDRESS);
          I2C_writeAnything(data_esp);
          Wire.endTransmission();

          if (Wire.requestFrom(SLAVE_ADDRESS, sizeof(data_nano)))
          {
            I2C_readAnything(data_nano);
          }
          data_mqtt.dustDensity = data_nano.dustDensity;

          balm_isr = false;
          //prevDisplay = now();
        }
      }

      if ((millis() - sentMills) > REPORT_INTERVAL )
      {
        payload = "{\"dustDensity\":";
        payload += data_nano.dustDensity;
        payload += ",\"moisture\":";
        payload += data_nano.moisture;
        payload += ",\"Humidity\":";
        payload += data_mqtt.humidity;
        payload += ",\"Temperature\":";
        payload += data_mqtt.tempinside2;
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

        sendmqttMsg(topic, payload);

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
      client.loop();
    }
    ArduinoOTA.handle();
  }
  else
  {
    wifi_connect();
  }
}

void displayHost(int numofhost, int numofall)
{
  lcd.setCursor(17, 2);
  if (numofall < 10)
  {
    lcd.print(' ');
  }
  lcd.print(numofall);

  lcd.setCursor(15, 2);
  lcd.print(numofhost);
}

void displayPIR(int PIR)
{
  if ( PIR == 1)
  {
    for ( int i = 0 ; i <= 3 ; i ++ )
    {
      lcd.setCursor(19, i);
      lcd.write(5);
    }
  }
  else
  {
    for ( int i = 0 ; i <= 3 ; i ++ )
    {
      lcd.setCursor(19, i);
      lcd.print(" ");
    }
  }
}

void displayNemoWeight(int nemoWeight)
{
  String str_nemoWeight = String(nemoWeight);
  int length_nemoWeight = str_nemoWeight.length();

  lcd.setCursor(2, 3);

  for ( int i = 0; i < ( 4 - length_nemoWeight ) ; i++ )
  {
    lcd.print(" ");
  }
  lcd.print(str_nemoWeight);
}

void displaypowerAvg(int Power)
{
  String str_Power = String(Power);
  int length_Power = str_Power.length();

  lcd.setCursor(10, 2);
  for ( int i = 0; i < ( 4 - length_Power ) ; i++ )
  {
    lcd.print(" ");
  }
  lcd.print(str_Power);
}

void displayTemperaturedigit(float Temperature)
{
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ )
  {
    lcd.print(" ");
  }
  lcd.print(Temperature, 1);
}

void displayTemperature() {
  lcd.setCursor(1, 1);
  displayTemperaturedigit((data_mqtt.tempinside1 + data_mqtt.tempinside2) / 2);

  lcd.setCursor(7, 1);

  if ( data_mqtt.tempoutside != -1000 ) {

    float tempdiff = data_mqtt.tempoutside - ((data_mqtt.tempinside1 + data_mqtt.tempinside2) / 2) ;
    displayTemperaturedigit(data_mqtt.tempoutside);

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
  if ( data_mqtt.humidity >= 10 ) {
    lcd.print(data_mqtt.humidity, 1);
  } else {
    lcd.print(" ");
    lcd.print(data_mqtt.humidity, 1);
  }
}

void digitalClockDisplay()
{
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

void printDigitsnocolon(int digits)
{
  if (digits < 10)
  {
    lcd.print('0');
  }
  lcd.print(digits);
}

void printDigits(int digits)
{
  lcd.print(":");
  if (digits < 10)
  {
    lcd.print('0');
  }
  lcd.print(digits);
}

void displaydustDensity(float dustDensity)
{
  int n = int(dustDensity / 0.05) ;

  if ( n > 9 )
  {
    n = 9 ;
  }

  for ( int i = 0 ; i < n ; i++)
  {
    lcd.setCursor(10 + i, 3);
    lcd.write(4);
  }

  for ( int o = 0 ; o < ( 9 - n) ; o++)
  {
    lcd.setCursor(10 + n + o, 3);
    lcd.print(".");
  }
}

void printEdgeTiming(class PietteTech_DHT * _d)
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
  sendNTPpacket(mqtt_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500)
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
