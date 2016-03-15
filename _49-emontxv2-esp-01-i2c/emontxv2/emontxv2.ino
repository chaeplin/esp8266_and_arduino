#include <Wire.h>
#include <RtcDS1307.h>
#include "PinChangeInterrupt.h"

/*
  atmega328  -  esp - ds1307
  3v
  gnd
  scl - d2  - scl
  sda - d0  - sda // led
  d12 -     - sqw
  d6  - tx
  d7  - rst
  d12 - rx  -     // button to inform flashing of esp
*/

// pins
// IN
#define SQW_PIN         2 // int 0
#define FLAH_ESP_PIN    12
// OUT
#define LED_PIN        9
#define NOTIFY_ESP_PIN 6
#define RESET_ESP_PIN  7
// Using SDA to put LOW D0 of ESP

RtcDS1307 Rtc;

volatile bool sqw_start;
volatile bool flash_start;

void SQW_CHECK() {
  sqw_start = true;
  digitalWrite(LED_PIN, HIGH);
}

void FLASH_STOP_CHECK() {
  detachPinChangeInterrupt(digitalPinToPinChangeInterrupt(SCL));
  Wire.begin();
}

void FLASH_START_CHECK() {
  flash_start = !flash_start;
  if (flash_start) {
    Wire.end();
    // PUT ESP to FLASH mode
    pinMode(SDA, OUTPUT);
    digitalWrite(SDA, LOW);

    // RESET ESP
    digitalWrite(RESET_ESP_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(RESET_ESP_PIN, HIGH);
    delayMicroseconds(10);
    
    // TO CHECK FLASHING IS DONE
    pinMode(SCL, INPUT_PULLUP);
    delayMicroseconds(10);
    attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(SCL), FLASH_STOP_CHECK, FALLING);
  } else {
    FLASH_STOP_CHECK();
  }
}

void start_measure() {
  delay(200);
  digitalWrite(LED_PIN, LOW);
}

void nofify_esp() {
  digitalWrite(NOTIFY_ESP_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(NOTIFY_ESP_PIN, HIGH);
}

void setup() {
  Rtc.Begin();
  Rtc.SetIsRunning(true);
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_1Hz);
  
  sqw_start = flash_start = false;

  // OUT
  pinMode(LED_PIN, OUTPUT);
  pinMode(NOTIFY_ESP_PIN, OUTPUT);
  pinMode(RESET_ESP_PIN, OUTPUT);

  // IN
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(FLAH_ESP_PIN, INPUT);

  //
  digitalWrite(NOTIFY_ESP_PIN, HIGH);
  digitalWrite(RESET_ESP_PIN, HIGH);

  attachInterrupt(digitalPinToInterrupt(SQW_PIN), SQW_CHECK, FALLING);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(FLAH_ESP_PIN), FLASH_START_CHECK, FALLING);
}

void loop() {
  if (sqw_start) {
    start_measure();
    if (!flash_start) {
      //write_nvram();
      nofify_esp();
    }
    sqw_start = false;
  }
}
