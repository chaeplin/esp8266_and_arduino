// Using Sonoff wifi switch, 80MHz, 1M
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include "/usr/local/src/rpi2_setting.h"

#define BUTTON_PIN 0
#define RELAY_PIN 12
#define LED_PIN 13

#define BETWEEN_RELAY_ACTIVE 5000

IPAddress mqtt_server = MQTT_SERVER;

char* subscribe_topic = "light/bedroomlight";
long lastReconnectAttempt = 0;
volatile bool bUpdated = false;
volatile bool bRelayState = false;
volatile bool bRelayReady = false; 
String clientName;
unsigned long lastRelayActionmillis;

void ICACHE_RAM_ATTR callback(char* intopic, byte* inpayload, unsigned int length);

WiFiClientSecure sslclient;
PubSubClient client(mqtt_server, 8883, callback, sslclient);
Ticker ticker;

void tick()
{
  //toggle state
  int state = digitalRead(LED_PIN);  // get the current state of GPIO13 pin
  digitalWrite(LED_PIN, !state);     // set pin to the opposite state
}

void ICACHE_RAM_ATTR parseMqttMsg(String receivedpayload, String receivedtopic)
{
  char json[] = "{\"cmd\":\"off\"}";

  receivedpayload.toCharArray(json, 150);
  StaticJsonBuffer<150> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success())
  {
    return;
  }

  if (receivedtopic == subscribe_topic)
  {
    if (root.containsKey("cmd"))
    {
      const char* mqtt_relay_state = root["cmd"];
      if ( String(mqtt_relay_state) == "on")
      {
        Serial.println("call on");
        bRelayState = HIGH;
      }
      else
      {
        Serial.println("call off");
        bRelayState = LOW;
      }
      bUpdated = true;
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

bool verifytls()
{
  Serial.print("[MQTT] tls connecting to ");
  Serial.println(mqtt_server);
  if (!sslclient.connect(mqtt_server, 8883))
  {
    Serial.println("[MQTT] tls connection failed");
    return false;
  }

  if (sslclient.verify(MQTT_FINGERPRINT, MQTT_SERVER_CN))
  {
    Serial.println("[MQTT] tls certificate matches");
    sslclient.stop();
    return true;
  }
  else
  {
    Serial.println("[MQTT] tls certificate doesn't match");
    sslclient.stop();
    return false;
  }
}

boolean reconnect()
{
  if (!client.connected())
  {
    if (verifytls())
    {
      if (client.connect((char*) clientName.c_str(), MQTT_USER, MQTT_PASS))
      {
        client.subscribe(subscribe_topic);
        client.loop();
        Serial.println("[MQTT] mqtt connected");
        ticker.attach(1, tick);
      }
      else
      {
        Serial.print("[MQTT] mqtt failed, rc=");
        Serial.println(client.state());
        ticker.attach(0.5, tick);
      }
    }
  }
  return client.connected();
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
    WiFi.hostname("esp-bedroomlight");

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

void change_light()
{
  digitalWrite(RELAY_PIN, bRelayState);
}

void run_lightcmd_isr()
{
  bRelayState = !bRelayState;
  bUpdated = true;
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

void ArduinoOTA_config()
{
  //OTA
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("esp-bedroomlight");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]()
  {
    ticker.attach(0.1, tick);
  });
  ArduinoOTA.onEnd([]()
  {
    ticker.attach(0.2, tick);
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

void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting....... ");
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lastRelayActionmillis = millis();

  attachInterrupt(BUTTON_PIN, run_lightcmd_isr, RISING);

  ticker.attach(0.2, tick);

  wifi_connect();

  clientName += "esp8266-";
  uint8_t mac[6];
  WiFi.macAddress(mac);
  clientName += macToStr(mac);
  clientName += "-";
  clientName += String(micros() & 0xff, 16);

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");

  ArduinoOTA_config();
  
  reconnect();
  lastReconnectAttempt = 0;
}

void loop()
{
  if ((millis() - lastRelayActionmillis) > BETWEEN_RELAY_ACTIVE)
  {
    bRelayReady = true;
  }
  else
  {
    bRelayReady = false;
  }

  if (bUpdated)
  {
    if (bRelayReady)
    {
      change_light();
      lastRelayActionmillis = millis();
      bUpdated = false;
    }
    else
    {
      bUpdated = false;
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
      client.loop();
    }
    ArduinoOTA.handle();
  }
  else
  {
    ticker.attach(0.2, tick);
    wifi_connect();
  }
}
// end of file
