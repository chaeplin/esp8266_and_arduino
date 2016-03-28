// base is _02-mqtt-sw-temperature
#include <TimeLib.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>

// 5pin
/* 24l01p    pro mini
   1  gnd   -
   2  vcc   +
   3  ce    9
   4  csn   10
   5  sck   13
   6  mosi  11
   7  miso  12
*/

#define CE_PIN 9
#define CSN_PIN 10

#define DEVICE_ID 25
#define CHANNEL 100

const uint64_t pipes[2] = { 0xFFFFFFFFCDLL };

uint32_t timestamp;

struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t data1;
  int16_t data2;
  uint8_t devid;
} sensor_data;

struct {
  uint32_t timestamp;
  uint16_t data1;
  uint16_t data2;
} data_ackpayload;

struct {
  uint32_t timestamp;
} time_reqpayload;

RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  Serial.begin(115200);

  time_reqpayload.timestamp = now();
  data_ackpayload.timestamp = now() ;
  data_ackpayload.data1     = 0;
  data_ackpayload.data2     = 0;

  Serial.println();
  digitalClockDisplay();

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(15, 15);
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  //radio.powerDown();

  setSyncProvider(getNrfTime);
}

time_t prevDisplay = 0;

void loop() {
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
    }
  }
}

void digitalClockDisplay() {
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits) {
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

time_t getNrfTime() {
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    radio.write(&time_reqpayload , sizeof(time_reqpayload));
    if (radio.isAckPayloadAvailable()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len == sizeof(data_ackpayload)) {
        radio.read(&data_ackpayload, sizeof(data_ackpayload));
        Serial.println(data_ackpayload.timestamp);
      }
    }

    time_reqpayload.timestamp = data_ackpayload.timestamp;
    radio.write(&time_reqpayload , sizeof(time_reqpayload));
    if (radio.isAckPayloadAvailable()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len == sizeof(data_ackpayload)) {
        radio.read(&data_ackpayload, sizeof(data_ackpayload));
        Serial.println(data_ackpayload.timestamp);
      }
    }

    time_reqpayload.timestamp = data_ackpayload.timestamp;
    radio.write(&time_reqpayload , sizeof(time_reqpayload));
    if (radio.isAckPayloadAvailable()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len == sizeof(data_ackpayload)) {
        radio.read(&data_ackpayload, sizeof(data_ackpayload));
        Serial.println(data_ackpayload.timestamp);
        return (unsigned long)data_ackpayload.timestamp;
      }
    }
  }
  Serial.println("No NTP Response :-(");
  return 0;
}
