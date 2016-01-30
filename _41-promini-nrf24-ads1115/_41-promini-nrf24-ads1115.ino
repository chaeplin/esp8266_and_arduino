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

/*
   17  / int 0    ---> record value to influxdb
   16 / 15 ---> detect range ( m / u / n )
*/

/*
  D0    PCINT16 (PCMSK2 / PCIF2 / PCIE2)
  D1    PCINT17 (PCMSK2 / PCIF2 / PCIE2)
  D2    PCINT18 (PCMSK2 / PCIF2 / PCIE2)
  D3    PCINT19 (PCMSK2 / PCIF2 / PCIE2)
  D4    PCINT20 (PCMSK2 / PCIF2 / PCIE2)
  D5    PCINT21 (PCMSK2 / PCIF2 / PCIE2)
  D6    PCINT22 (PCMSK2 / PCIF2 / PCIE2)
  D7    PCINT23 (PCMSK2 / PCIF2 / PCIE2)
  D8    PCINT0  (PCMSK0 / PCIF0 / PCIE0)
  D9    PCINT1  (PCMSK0 / PCIF0 / PCIE0)
  D10   PCINT2  (PCMSK0 / PCIF0 / PCIE0)
  D11   PCINT3  (PCMSK0 / PCIF0 / PCIE0)
  D12   PCINT4  (PCMSK0 / PCIF0 / PCIE0)
  D13   PCINT5  (PCMSK0 / PCIF0 / PCIE0)
  A0    PCINT8  (PCMSK1 / PCIF1 / PCIE1)
  A1    PCINT9  (PCMSK1 / PCIF1 / PCIE1)
  A2    PCINT10 (PCMSK1 / PCIF1 / PCIE1)
  A3    PCINT11 (PCMSK1 / PCIF1 / PCIE1)
  A4    PCINT12 (PCMSK1 / PCIF1 / PCIE1)
  A5    PCINT13 (PCMSK1 / PCIF1 / PCIE1)


  ISR (PCINT0_vect)
  {
  // handle pin change interrupt for D8 to D13 here
  }  // end of PCINT0_vect

  ISR (PCINT1_vect)
  {
  // handle pin change interrupt for A0 to A5 here
  }  // end of PCINT1_vect

  ISR (PCINT2_vect)
  {
  // handle pin change interrupt for D0 to D7 here
  }  // end of PCINT2_vect
*/

#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <SPI.h>
#include <LowPower.h>
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

const uint64_t pipes[2] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL };

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

// analogPin 3 / 2 / 1 / int 0
const int wakeupPin = 2;
const int recordPin = 17;
const int nanoPin   = 16;
const int microPin  = 15;
const int ledPin    = 14;

int rangeStatus;

unsigned long counterForloop;
volatile unsigned long counterForloop_old;

void wakeUp() {
  counterForloop_old = counterForloop;
  detachInterrupt(digitalPinToInterrupt(2));
}

void sleepNow() {
  digitalWrite(ledPin, LOW);
  attachInterrupt(digitalPinToInterrupt(2), wakeUp, FALLING);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void checkBttn() {
  attachInterrupt(digitalPinToInterrupt(2), sleepNow, FALLING);
}

void setup() {
  Serial.begin(115200);

  delay(20);
  unsigned long startmilis = millis();
  adc_disable();

  pinMode(recordPin, INPUT_PULLUP);
  pinMode(wakeupPin, INPUT_PULLUP);
  pinMode(microPin, INPUT_PULLUP);
  pinMode(nanoPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  int microStatus = digitalRead(microPin);
  int nanoStatus  = digitalRead(nanoPin);

  rangeStatus = microStatus << 1;
  rangeStatus = rangeStatus + nanoStatus;

  counterForloop = 0;

  // radio
  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  radio.setPayloadSize(11);
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  radio.powerDown();

  // payload
  unsigned long stopmilis = millis();
  payload.data2 = ( stopmilis - startmilis ) * 10 ;

  payload._salt = 0;
  payload.devid = DEVICE_ID;

  sleepNow();
}


void loop() {
  digitalWrite(ledPin, HIGH);

  int microStatus = digitalRead(microPin);
  int nanoStatus  = digitalRead(nanoPin);

  rangeStatus = microStatus << 1;
  rangeStatus = rangeStatus + nanoStatus;

  Serial.print("counterForloop : ");
  Serial.print(counterForloop);
  Serial.print(" - rangeStatus : ");
  Serial.println(rangeStatus);

  if (counterForloop > (counterForloop_old + 10)) {
    checkBttn();
  }


  // milli
  // micro
  // nano



  /*
    unsigned long startmilis = millis();

    payload._salt++;
    payload.data1 = 10 ;
    payload.volt = readVcc();

    radio.powerUp();
    radio.write(&payload , sizeof(payload));
    radio.powerDown();

    unsigned long stopmilis = millis();

    payload.data2 = ( stopmilis - startmilis ) * 10 ;
  */
  digitalWrite(ledPin, LOW);
  counterForloop++;
  delay(100);
}

int readVcc() {
  adc_enable();
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
#if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#else
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#endif

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  adc_disable();

  return (int)result; // Vcc in millivolts
}

