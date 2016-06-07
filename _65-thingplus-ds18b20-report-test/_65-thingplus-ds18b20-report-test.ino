#include <OneWire.h>
#include <DallasTemperature.h>
// https://github.com/daliworks/arduino_library
#include <Thingplus.h>
#include "Ntp.h"
#include <TimeLib.h>

//FIXME WIFI SSID / PASSWORD
#include "/usr/local/src/ap_setting.h"

#define SYS_CPU_80MHz 80
#define SYS_CPU_160MHz 160

extern "C" {
#include "user_interface.h"
}

// thing+ setting
#include "/usr/local/src/thingplus_setting.h"

// using gpio pin for ds18b20
#define VCC_FOR_ONE_WIRE_BUS 5
#define ONE_WIRE_BUS 4
#define GND_FOR_ONE_WIRE_BUS 0

#define TEMPERATURE_PRECISION 9

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

//////////////////////////////////////////////////////////////////
//byte mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};     //FIXME MAC ADDRESS
const char *apikey = THINGP_APIKEY;  	       //FIXME APIKEY
const char *ledId  = THINGP_LEDID;			       //FIXME LED ID
const char *temperatureId = THINGP_TEMPID;    //FIXME TEMPERATURE ID
//////////////////////////////////////////////////////////////////

int LED_GPIO = 2;
int reportIntervalSec = 60;

char* actuatingCallback(const char *id, const char *cmd, const char *options) {
  if (strcmp(id, ledId) == 0) {
    if (strcmp(cmd, "on") == 0) {
      digitalWrite(LED_GPIO, HIGH);
      return "success";
    }
    else  if (strcmp(cmd, "off") == 0) {
      digitalWrite(LED_GPIO, LOW);
      return "success";
    }
  }

  return NULL;
}


void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);

  switch (event) {
    case WIFI_EVENT_STAMODE_GOT_IP:
      Serial.println();
      Serial.print("[WIFI] connected : ");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      /*
        Serial.println(WiFi.status());
        Serial.println(WiFi.waitForConnectResult());
      */
      break;
    case WIFI_EVENT_STAMODE_DISCONNECTED:
      Serial.println("[WIFI] lost connection");
      break;
  }
}

void setup() {
  system_update_cpu_freq(SYS_CPU_160MHz);
  //system_update_cpu_freq(SYS_CPU_80MHz);
  pinMode(LED_GPIO, OUTPUT);

  // WIFI
  WiFi.onEvent(WiFiEvent);
  wifi_set_phy_mode(PHY_MODE_11N);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // ds18b20
  pinMode(VCC_FOR_ONE_WIRE_BUS, OUTPUT);
  pinMode(GND_FOR_ONE_WIRE_BUS, OUTPUT);

  digitalWrite(VCC_FOR_ONE_WIRE_BUS, HIGH);
  digitalWrite(GND_FOR_ONE_WIRE_BUS, LOW);

  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }

  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) {
    Serial.println("sensor fail");
  }
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);

  uint8_t mac[6];
  WiFi.macAddress(mac);

  if (String(apikey) == "") {

    Serial.println();
    Serial.println();
    Serial.println("Please register this device");
    Serial.print("Wifi mac address is : ");
    Serial.println(macToStr(mac));
    Serial.flush();
    while (1) {
      yield();
    }
  }

  Serial.println("connecting to WIFI");
  while (WiFi.status() != 3) {
    Serial.print(". ");
    delay(100);
  }

  while ( timeStatus() == timeNotSet ) {
    NtpBegin();
    NtpSync();
    delay(100);
  }


  Thingplus.begin(mac, apikey);
  Serial.println(WiFi.localIP());
  Thingplus.setActuatingCallback(actuatingCallback);
}

time_t current;
time_t nextReportInterval = now();

float temperatureGet() {
  float temperature;
  sensors.requestTemperatures();
  temperature = sensors.getTempC(insideThermometer);
  //Serial.print("Temp is : ");
  //Serial.println(temperature);
  return temperature;
}

void loop() {
  if (WiFi.status() == 3) {
    Thingplus.keepConnect();

    current = now();
    if (current > nextReportInterval) {
      Thingplus.gatewayStatusPublish(true, reportIntervalSec * 3);
      Thingplus.sensorStatusPublish(temperatureId, true, reportIntervalSec * 3);
      Thingplus.valuePublish(temperatureId, temperatureGet());
      nextReportInterval = current + reportIntervalSec;
    }
  }
}

String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
  }
  return result;
}
