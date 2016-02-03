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

#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <printf.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CE_PIN 9
#define CSN_PIN 10

#define DEVICE_ID 18
#define CHANNEL 50

const uint64_t pipes[3] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL, 0xFFFFFFFFCCLL };

typedef struct {
  uint32_t _salt;
  uint32_t data1;
  uint32_t data2;
  uint32_t data3;
  uint32_t data4;
} data;

data payload;

RF24 radio(CE_PIN, CSN_PIN);

const int doRecordPin = 3;
const int ledPin      = 14;

volatile int recordNo;
volatile boolean dorecored;

unsigned long loopcount;

void record() {
  noInterrupts ();
  recordNo++;
  dorecored = !dorecored;
  interrupts ();
}

void setup() {
  Serial.begin(115200);
  printf_begin();
  delay(20);
  adc_disable();
  pinMode(doRecordPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);

  loopcount = 0;
  recordNo = 0;
  dorecored = false;

  // radio
  radio.begin();
  radio.setChannel(CHANNEL);
  //radio.setPALevel(RF24_PA_LOW);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  radio.enableDynamicPayloads();
  //radio.enableDynamicAck();
  radio.openWritingPipe(pipes[2]);
  radio.stopListening();
  //radio.powerDown();

  payload._salt = 0;

  for (int k = 0; k < 10; k = k + 1) {
    if (k % 2 == 0) {
      digitalWrite(ledPin, HIGH);
    }
    else {
      digitalWrite(ledPin, LOW);
    }
    delay(100);
  }
  radio.printDetails();

  attachInterrupt(digitalPinToInterrupt(3), record, RISING);
}

void loop() {
  if (dorecored == true) {
    //startmilis = millis();
    digitalWrite(ledPin, HIGH);

    payload.data1 = loopcount;
    payload.data2 = recordNo;
    payload.data3 = loopcount;
    payload.data4 = recordNo;

    Serial.print(sizeof(payload));
    radio.write(&payload, sizeof(payload));

    digitalWrite(ledPin, LOW);
    loopcount++;
    payload._salt++;

  } else {
    digitalWrite(ledPin, LOW);
  }
  delay(1000);
}
