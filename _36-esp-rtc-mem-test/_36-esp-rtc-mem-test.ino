/*
  system_rtc_mem_write
  Function:
  During deep sleep, only RTC still working, so maybe we need to save some user data in RTC memory.
  Only user data area can be used by user.
  |<--------system data--------->|<-----------------user data--------------->|
  | 256 bytes                    | 512 bytes                                 |

  Note:
  RTC memory is 4 bytes aligned for read and write operations. Parameter des_addr means block number(4 bytes per block).
  So, if we want to save some data at the beginning of user data area, des_addr will be 256/4 = 64, save_size will be data length.

  Prototype:
  bool system_rtc_mem_write (
    uint32 des_addr,
    void * src_addr,
    uint32 save_size
  )

  Parameter:
    uint32 des_addr : destination address (block number) in RTC memory, des_addr >=64
    void * src_addr : data pointer.
    uint32 save_size : data length ( byte)

  Return:
    true: succeed
    false: fail


  system_rtc_mem_read
  Function:
  Read user data from RTC memory. Only user data area should be accessed by the user.
  |<--------system data--------->|<-----------------user data--------------->|
  | 256 bytes                    | 512 bytes                                 |

  Note:
  RTC memory is 4 bytes aligned for read and write operations.
  Parameter src_addr means block number(4 bytes per block).
  So, to read data from the beginning of user data area, src_addr will be 256/4=64, save_size will be data length.

  Prototype:
  bool system_rtc_mem_read (
    uint32 src_addr,
    void * des_addr,
    uint32 save_size
  )

  Parameter:
    uint32 src_addr : source address (block number) in rtc memory, src_addr >= 64
    void * des_addr : data pointer
    uint32 save_size : data length, byte

  Return:
    true: succeed
    false: fail
*/

// dest and src_addr will start from 65

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "/usr/local/src/ap_setting.h"

extern "C" {
#include "user_interface.h"
}

//--
#define RTC_MAGIC 12345

typedef struct _tagPoint {
  uint32 magic ;
  uint32 salt;
  uint32 nemo;
} RTC_TEST;

RTC_TEST rtc_mem_test;

//---
String macToStr(const uint8_t* mac);
void sendmqttMsg(char* topictosend, String payload);
void goingToSleep();
//---
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
IPAddress mqtt_server = MQTT_SERVER;

char* topic = "pubtest";

String clientName;
long lastReconnectAttempt = 0;
long lastMsg = 0;
int test_para = 2000;
unsigned long startMills;

WiFiClient wifiClient;
PubSubClient client(mqtt_server, 1883, wifiClient);

void goingToSleepNoRF()
{
  // 3 sec
  Serial.println("going to sleep with no rf");
  //system_deep_sleep_set_option(4);
  //system_deep_sleep(10000000);
  ESP.deepSleep(1, WAKE_RF_DISABLED);
}

void goingToSleepWithRF()
{
  // 3 sec
  Serial.println("going to sleep with rf");
  //system_deep_sleep_set_option(0);
  //system_deep_sleep(10000000);
  ESP.deepSleep(3000000, WAKE_RF_DEFAULT);
}

void rtc_count()
{
  // system_rtc_mem_read(64... not work, use > 64
  system_rtc_mem_read(100, &rtc_mem_test, sizeof(rtc_mem_test));
  Serial.print("=================> rtc mem mgic / salt / nemo :  ");
  Serial.print(rtc_mem_test.magic);
  Serial.print(" / ");
  Serial.print(rtc_mem_test.salt);
  Serial.print(" / ");
  Serial.println(rtc_mem_test.nemo);
  if (rtc_mem_test.magic != RTC_MAGIC) {
    Serial.println("===============> rtc mem init...");
    rtc_mem_test.magic = RTC_MAGIC;
    rtc_mem_test.salt = 0;
    rtc_mem_test.nemo = 0;
  }

  rtc_mem_test.salt++;
  rtc_mem_test.nemo++;

  boolean reportnow = false;
  if ( rtc_mem_test.nemo > 10 || rtc_mem_test.nemo == 1) {
    reportnow = true;
    rtc_mem_test.nemo = 1;
  }

  if (system_rtc_mem_write(100, &rtc_mem_test, sizeof(rtc_mem_test))) {
    Serial.println("rtc mem write is ok");
  } else {
    Serial.println("rtc mem write is fail");
  }
  if (reportnow == false) {
    if ( rtc_mem_test.nemo == 10 ) {
      goingToSleepWithRF();
    } else {
      goingToSleepNoRF();
    }
  }
}

boolean reconnect()
{
  if (!client.connected()) {
    if (client.connect((char*) clientName.c_str())) {
      Serial.println("===> mqtt connected");
    } else {
      Serial.print("---> mqtt failed, rc=");
      Serial.println(client.state());
    }
  }
  return client.connected();
}

void wifi_connect()
{
  if (WiFi.status() != WL_CONNECTED) {
    // WIFI
    Serial.println();
    Serial.print("===> WIFI ---> Connecting to ");
    Serial.println(ssid);
    delay(10);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int Attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(". ");
      Serial.print(Attempt);
      delay(100);
      Attempt++;
      if (Attempt == 250)
      {
        Serial.println();
        Serial.println("-----> Could not connect to WIFI");
        ESP.restart();
        delay(200);
      }

    }
    Serial.println();
    Serial.print("===> WiFi connected");
    Serial.print(" ------> IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void setup()
{
  startMills = millis();
  Serial.begin(74880);

  Serial.println("");
  Serial.println("rtc mem test");
  Serial.println(wifi_station_get_auto_connect());
  WiFi.setAutoConnect(true);

  rtc_count();

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

}

void loop()
{
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      long now = millis();
      if (now - lastReconnectAttempt > 200) {
        lastReconnectAttempt = now;
        if (reconnect()) {
          lastReconnectAttempt = 0;
        }
      }
    } else {
      long now = millis();
      if (now - lastMsg > test_para) {
        lastMsg = now;
        String payload = "{\"startMills\":";
        payload += (millis() - startMills);
        payload += ",\"salt\":";
        payload += rtc_mem_test.salt;        
        payload += ",\"FreeHeap\":";
        payload += ESP.getFreeHeap();
        payload += ",\"RSSI\":";
        payload += WiFi.RSSI();
        payload += "}";
        sendmqttMsg(topic, payload);
      }
      client.loop();
      goingToSleepNoRF();
    }
  } else {
    wifi_connect();
  }
}

void sendmqttMsg(char* topictosend, String payload)
{

  if (client.connected()) {
    Serial.print("Sending payload: ");
    Serial.print(payload);

    unsigned int msg_length = payload.length();

    Serial.print(" length: ");
    Serial.println(msg_length);

    byte* p = (byte*)malloc(msg_length);
    memcpy(p, (char*) payload.c_str(), msg_length);

    if ( client.publish(topictosend, p, msg_length)) {
      Serial.println("Publish ok");
      free(p);
      //return 1;
    } else {
      Serial.println("Publish failed");
      free(p);
      //return 0;
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
