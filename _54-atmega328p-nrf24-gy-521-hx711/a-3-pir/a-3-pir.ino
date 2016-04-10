// base is _02-mqtt-sw-temperature
// 5pin
/* 24l01p    pro mini/mega328
   1  gnd   -
   2  vcc   +
   3  ce    9
   4  csn   10
   5  sck   13
   6  mosi  11
   7  miso  12
  //-
  int 0 --> int of gy-521
  int 1 --> int of pir
  //
  A5/19 --> Wire SCL
  A4/18 --> Wire SDA
  A3/17 --> LED
  A2/16 --> VCC of PIR
  A0/14 --> VCC of gy-521
*/
#include <LowPower.h>
#include <TimeLib.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
//
#include "I2Cdev.h"
#include "MPU6050.h"
//
#include "HX711.h"

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// http://www.gammon.com.au/forum/?id=12769
#if defined(__AVR_ATtiny85__)
#define watchdogRegister WDTCR
#else
#define watchdogRegister WDTCSR
#endif

// PINS
#define LED       17
#define PIR_VCC   16
#define PIR_INT   3
#define GY521_VCC 14
#define GY521_INT 2

// nrf24
#define CE_PIN    9
#define CSN_PIN   10

// HX711
#define HX711_SCK 8
#define HX711_DT  7

// nrf24
#define DEVICE_ID 35
#define CHANNEL   100

volatile bool bpir_isr;

void pir_isr_start() {
  attachInterrupt(digitalPinToInterrupt(PIR_INT), pir_isr_stop, RISING);
}

void pir_isr_stop() {
  detachInterrupt(digitalPinToInterrupt(PIR_INT));
  bpir_isr = true;
}

void pir_isr_initialize() {
  detachInterrupt(digitalPinToInterrupt(PIR_INT));
  attachInterrupt(digitalPinToInterrupt(PIR_INT), pir_isr_stop, RISING);
}

void setup() {
  wdt_enable(WDTO_8S);
  bpir_isr = false;

  Serial.begin(115200);
  delay(100);

  pinMode(PIR_VCC, OUTPUT);
  pinMode(PIR_INT, INPUT_PULLUP);

  digitalWrite(PIR_VCC, HIGH);

  Serial.println("Sensor started");


  attachInterrupt(digitalPinToInterrupt(PIR_INT), pir_isr_initialize, FALLING);
}

void loop() {
  wdt_reset();
  if (bpir_isr) {
    Serial.println("pir detected");
    pir_isr_start();
    bpir_isr = false;
  }

}
//
