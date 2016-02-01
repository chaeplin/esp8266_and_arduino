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
#include <Wire.h>
#include <Adafruit_ADS1015.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CE_PIN 9
#define CSN_PIN 10

#define DEVICE_ID 15
#define CHANNEL 100

const uint64_t pipes[3] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL, 0xFFFFFFFFCCLL };

typedef struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t data1;
  int16_t data2;
  uint8_t devid;
} data;

data payload;

RF24 radio(CE_PIN, CSN_PIN);
Adafruit_ADS1115 ads(0x48);

const int doRecordPin = 3;
const int nanoPin     = 16;
const int microPin    = 15;
const int ledPin      = 14;

int rangeStatus;
volatile int recordNo;
volatile boolean dorecored;
//unsigned long startmilis;
//unsigned long stopmilis;
unsigned long loopcount;

void record() {
  recordNo++;
  dorecored = !dorecored;
}

void setup() {
  delay(20);
  adc_disable();
  pinMode(doRecordPin, INPUT);
  pinMode(microPin, INPUT_PULLUP);
  pinMode(nanoPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);

  int microStatus = digitalRead(microPin);
  int nanoStatus  = digitalRead(nanoPin);

  rangeStatus = microStatus << 1;
  rangeStatus = rangeStatus + nanoStatus;

  //startmilis = 0;
  //stopmilis = 0;
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
  radio.setPayloadSize(11);
  radio.openWritingPipe(pipes[2]);
  radio.stopListening();
  //radio.powerDown();

  payload._salt = 0;
  payload.devid = DEVICE_ID;
  payload.volt  = 0;

  ads.setGain(GAIN_ONE);
  ads.begin();
  ads.readADC_Differential_0_1_no_delay();

  for (int k = 0; k < 10; k = k + 1) {
    if (k % 2 == 0) {
      digitalWrite(ledPin, HIGH);
    }
    else {
      digitalWrite(ledPin, LOW);
    }
    delay(100);
  }
  attachInterrupt(digitalPinToInterrupt(3), record, RISING);
}

void loop() {
  if (dorecored == true) {
    //startmilis = millis();
    digitalWrite(ledPin, HIGH);
    int microStatus = digitalRead(microPin);
    int nanoStatus  = digitalRead(nanoPin);

    rangeStatus = microStatus << 1;
    rangeStatus = rangeStatus + nanoStatus;

    int16_t results;
    float multiplier = 0.125F;
    //results = ads.readADC_Differential_0_1();
    results = ads.getLastConversionResults_no_delay();

    payload.data1 = results * multiplier ;
    payload.data2 = rangeStatus;
    payload.volt = loopcount;
    payload._salt = recordNo ;

    ads.readADC_Differential_0_1_no_delay();
    
    radio.write(&payload, sizeof(payload));
    
    digitalWrite(ledPin, LOW);
    //stopmilis = millis();
    //payload.volt = ( stopmilis - startmilis ) ;
    loopcount++;
  } else {
    digitalWrite(ledPin, LOW);
  }
}
