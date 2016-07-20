//
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <lgWhisen.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

#define IR_TX_PIN 4
#define AC_CONF_TYPE 1    // 0: tower, 1: wall
#define AC_CONF_HEATING 0   // 0: cooling, 1: heating
#define AC_DEFAULT_TEMP 27  // temp 18 ~ 30
#define AC_DEFAULT_FLOW 1   // fan speed 0: low, 1: mid, 2: high

#define REPORT_INTERVAL 5000 // in msec
#define MAX_PIR_TIME 1800000  // ms 30 min
#define PIR_INT 14

#include "/usr/local/src/ap_setting.h"

char* subscribe_cmd   = "esp8266/cmd/ac";
char* subscribe_set   = "esp8266/cmd/acset";
char* reporting_topic = "report/pirtest";

volatile struct
{
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
  uint8_t ac_presence_mode;
  bool haveData;
} ir_data;

IPAddress mqtt_server = MQTT_SERVER;

String clientName;
long lastReconnectAttempt = 0;

unsigned int localPort = 12390;
const int timeZone = 9;
time_t prevDisplay = 0;

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length);

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, callback, wifiClient);
lgWhisen lgWhisen;
WiFiUDP udp;

bool bpresence = false;
volatile bool bpir_isr;
volatile uint32_t pir_interuptCount = 0;
unsigned long lastpirMillis = 0;
unsigned long startMills;

void ICACHE_RAM_ATTR pir_isr() {
  pir_interuptCount++;
  bpir_isr = true;
}

bool ICACHE_RAM_ATTR sendmqttMsg(char* topictosend, String payloadtosend, bool retain = false)
{
  unsigned int msg_length = payloadtosend.length();

  byte* p = (byte*)malloc(msg_length);
  memcpy(p, (char*) payloadtosend.c_str(), msg_length);

  if (client.publish(topictosend, p, msg_length, retain))
  {
    free(p);
    client.loop();

    Serial.print("[MQTT] out topic : ");
    Serial.print(topictosend);
    Serial.print(" payload: ");
    Serial.print(payloadtosend);
    Serial.println(" published");

    return true;

  } else {
    free(p);
    client.loop();

    Serial.print("[MQTT] out topic : ");
    Serial.print(topictosend);
    Serial.print(" payload: ");
    Serial.print(payloadtosend);
    Serial.println(" publish failed");

    return false;
  }
}

void ICACHE_RAM_ATTR sendCheck()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["pir_interuptCount"] = pir_interuptCount;
  root["presence"]          = uint8_t(bpresence);
  root["ac_mode"]           = ir_data.ac_mode;
  root["ac_presence_mode"]  = ir_data.ac_presence_mode;
  String json;
  root.printTo(json);

  sendmqttMsg(reporting_topic, json, false);
}

void ICACHE_RAM_ATTR parseMqttMsg(String receivedpayload, String receivedtopic)
{
  char json[] = "{\"AC\":0,\"ac_temp\":27,\"ac_flow\":1}";

  receivedpayload.toCharArray(json, 150);
  StaticJsonBuffer<150> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success())
  {
    return;
  }

  if (receivedtopic == subscribe_cmd)
  {
    if (root.containsKey("AC"))
    {
      ir_data.ac_mode = root["AC"];
      if (bpresence)
      {
        ir_data.haveData = true;
      }
    }
  }

  if (receivedtopic == subscribe_set)
  {
    if (root.containsKey("ac_temp"))
    {
     if (ir_data.ac_temp != root["ac_temp"])
     {
        ir_data.ac_temp = root["ac_temp"];
        /*
            if (root.containsKey("ac_flow"))
            {
                ir_data.ac_flow = root["ac_flow"];
            }
        */

        if (ir_data.ac_mode == 1 && bpresence)
        {
            ir_data.haveData = true;
        }
      }
    }
  }
}

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length)
{
  String receivedtopic = intopic;
  String receivedpayload ;

  for (unsigned int i = 0; i < length; i++)
  {
    receivedpayload += (char)inpayload[i];
  }

  Serial.print("[MQTT] intopic : ");
  Serial.print(receivedtopic);
  Serial.print(" payload: ");
  Serial.println(receivedpayload);

  parseMqttMsg(receivedpayload, receivedtopic);
}

boolean reconnect()
{
  if (!client.connected())
  {
    if (client.connect((char*) clientName.c_str()))
    {
      client.subscribe(subscribe_cmd);
      client.loop();
      client.subscribe(subscribe_set);
      client.loop();
      Serial.println("[MQTT] mqtt connected");
    }
    else
    {
      Serial.print("[MQTT] mqtt failed, rc=");
      Serial.println(client.state());
    }
  }
  return client.connected();
}

void ArduinoOTA_config()
{
  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-irbedroom");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]()
  {
    //sendUdpSyslog("ArduinoOTA Start");
  });
  ArduinoOTA.onEnd([]()
  {
    //sendUdpSyslog("ArduinoOTA End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
  {
    //syslogPayload = "Progress: ";
    //syslogPayload += (progress / (total / 100));
    //sendUdpSyslog(syslogPayload);
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
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

void sendNTPpacket(IPAddress & address)
{
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

time_t getNtpTime()
{
  while (udp.parsePacket() > 0) ;
  sendNTPpacket(mqtt_server);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500)
  {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
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

void printDigits(int digits)
{
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay()
{
  Serial.print("[TIME] ");
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.println(year());
}

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println();
    Serial.print("[WIFI] Connecting to ");
    Serial.println(WIFI_SSID);

    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
      Serial.print(". ");
      Serial.print(Attempt);
      delay(100);
      Attempt++;
      if (Attempt == 150)
      {
        Serial.println();
        Serial.println("[WIFI] Could not connect to WIFI, restarting...");
        Serial.flush();
        ESP.restart();
        delay(200);
      }
    }

    Serial.println();
    Serial.print("[WIFI] connected");
    Serial.print(" --> IP address: ");
    Serial.println(WiFi.localIP());
  }
}

String macToStr(const uint8_t* mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting....... ");

  ir_data.ac_mode          = 0;
  ir_data.ac_temp          = AC_DEFAULT_TEMP;
  ir_data.ac_flow          = AC_DEFAULT_FLOW;
  ir_data.haveData         = false;
  ir_data.ac_presence_mode = 0;

  lgWhisen.setActype(AC_CONF_TYPE);
  lgWhisen.setHeating(AC_CONF_HEATING);
  lgWhisen.setTemp(ir_data.ac_temp);
  lgWhisen.setFlow(ir_data.ac_flow);
  lgWhisen.setIrpin(IR_TX_PIN);

  pinMode(PIR_INT, INPUT);
  bpir_isr = false;
  attachInterrupt(PIR_INT, pir_isr, RISING);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  // ntp update
  udp.begin(localPort);
  if (timeStatus() == timeNotSet)
  {
    Serial.println("[NTP] get ntp time");
    setSyncProvider(getNtpTime);
    delay(500);
  }

  // start with ac power down
  lgWhisen.power_down();

  ArduinoOTA_config();

  reconnect();
  lastReconnectAttempt = 0;
  startMills = millis();
}

void loop()
{
  if (bpir_isr)
  {
    Serial.println("[PIR] ---> pir detected");
    lastpirMillis = millis();
    bpir_isr = false;
  }

  if ((millis() - lastpirMillis < MAX_PIR_TIME) && ( lastpirMillis != 0))
  {
    bpresence = true;
  }
  else
  {
    bpresence = false;
  }  

  if (!bpresence && ir_data.ac_presence_mode == 1)
  {
    Serial.println("[IR] -----> AC Power Down");
    lgWhisen.power_down();
    ir_data.ac_presence_mode = 0;
  }

  if (bpresence && ir_data.ac_presence_mode == 0 && ir_data.ac_mode == 1)
  {
    ir_data.haveData = true;
  }

  if (now() != prevDisplay)
  {
    prevDisplay = now();
    if (timeStatus() == timeSet)
    {
      digitalClockDisplay();
    }
    else
    {
      Serial.println("[TIME] time not set");
    }
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      long now = millis();
      if (now - lastReconnectAttempt > 5000)
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
      // ac change, ir tx
      if (ir_data.haveData)
      {
        lgWhisen.setTemp(ir_data.ac_temp);
        lgWhisen.setFlow(ir_data.ac_flow);

        switch (ir_data.ac_mode)
        {
          // ac power down
          case 0:
            Serial.println("[IR] -----> AC Power Down");
            lgWhisen.power_down();
            break;

          // ac on
          case 1:
            Serial.println("[IR] -----> AC Power On");
            lgWhisen.activate();
            break;

          default:
            break;
        }
        ir_data.ac_presence_mode = ir_data.ac_mode;
        ir_data.haveData = false;
      }

      if ((millis() - startMills) > REPORT_INTERVAL) 
      {
        sendCheck();
        startMills = millis();
      }
      
      client.loop();
    }
  }
  else
  {
    wifi_connect();
  }
}

// end of file
