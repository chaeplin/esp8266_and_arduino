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
#include <printf.h>


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

#define DEVICE_ID 35
#define CHANNEL 100

const uint64_t pipes[2] = { 0xFFFFFFFFCDLL };

struct {
  uint32_t timestamp;
} time_ackpayload;

struct {
  uint32_t timestamp;
} time_reqpayload;

RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  Serial.begin(115200);
  printf_begin();

  time_reqpayload.timestamp = 0;
  time_ackpayload.timestamp = 0;

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

  radio.printDetails();
}

void loop() {
  getnrfdata();
  Serial.println(time_ackpayload.timestamp);
  delay(5000);
}

void getnrfdata() {
  time_reqpayload.timestamp = time_ackpayload.timestamp ;
  radio.write(&time_reqpayload , sizeof(time_reqpayload));
  if (radio.isAckPayloadAvailable()) {
    uint8_t len = radio.getDynamicPayloadSize();
    if ( len == sizeof(time_ackpayload)) {
      radio.read(&time_ackpayload, sizeof(time_ackpayload));
    }
  }
}
