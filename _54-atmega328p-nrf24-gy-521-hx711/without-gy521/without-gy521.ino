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
  int 1 --> int of pir
  //
  A5/19 --> Wire SCL
  A4/18 --> Wire SDA
  A3/17 --> LED
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
#include <Average.h>

//
#include "HX711.h"

#define DEBUG_PRINT 0

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable() (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// PINS
#define LED       17
#define PIR_INT   3

// nrf24
#define CE_PIN    9
#define CSN_PIN   10

// HX711
#define HX711_SCK 8
#define HX711_DT  7

// nrf24
#define DEVICE_ID 35
#define CHANNEL   100

const uint64_t pipes[1] = { 0xFFFFFFFFDDLL };

struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t avemean;
  int16_t avestddev;
  uint8_t avetype;
  uint8_t devid;
} scale_payload;

RF24 radio(CE_PIN, CSN_PIN);
HX711 scale(HX711_DT, HX711_SCK);
Average<float> ave(10);

unsigned int detcdur = 3;
unsigned int thrs    = 1;

volatile bool bpir_isr;
bool blowpower;

void pir_isr() {
  bpir_isr = true;
}

void setup() {
  //
  pinMode(LED, OUTPUT);
  // INT
  pinMode(PIR_INT, INPUT); //_PULLUP);
  //
  adc_disable();
  //
  ledonoff(8, 5);

  if (DEBUG_PRINT) {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println("00 --> Starting unit : wire, gyro, pir, hx711, radio");
  }

  Wire.begin();

  // radio
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
  radio.powerDown();

  scale_payload._salt = 0;
  scale_payload.volt  = readVcc();
  scale_payload.devid = DEVICE_ID;

  bpir_isr = blowpower = false;

  scale.set_scale(23500.f);
  scale.tare(10);
  scale.power_down();

  digitalWrite(LED, LOW);
}

/*
  avetype
  4 : before tare
  3 : after pir
  2 : low power
  1 : average
  0 : every
*/

void loop() {
  goingSleep();

  if (bpir_isr) {
    if (DEBUG_PRINT) {
      Serial.println("02 ---> pir detected --> checking hx711");
    }

    ledonoff(4, 1);

    int nofchecked = 0;
    while (1) {

      int16_t nWeight = gethx711(2);

      if (DEBUG_PRINT) {
        Serial.print("02 ---> checking nWeight : ");
        Serial.print(nWeight);
        Serial.print(" count : ");
        Serial.println(nofchecked);
      }

      if ( nofchecked == 0 ) {
        scale_payload.avemean   = nWeight;
        scale_payload.avestddev = 0;
        scale_payload.avetype   = 3; // after pir
        radio.powerUp();
        radio.write(&scale_payload, sizeof(scale_payload));
        radio.powerDown();
      }

      if ( nWeight > 1500 ) {
        nemoison();
        break;
      }

      nofchecked++;
      if ( nofchecked > 2 ) {
        break;
      }
      LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
      ledonoff(2, 2);
    }

    scale_payload.volt = readVcc();
    // alarm low power
    if (!blowpower && (scale_payload.volt < 2500)) {
      scale_payload.avemean   = 0;
      scale_payload.avestddev = 0;
      scale_payload.avetype   = 2; // low power
      radio.powerUp();
      radio.write(&scale_payload, sizeof(scale_payload));
      radio.powerDown();
      blowpower = true;
    }
  }

  if (DEBUG_PRINT) {
    Serial.println("05 ---> start again");
    Serial.println();
  }
  scale_payload._salt++;
  ledonoff(4, 2);
}

void nemoison() {
  int nofchecked = 0;
  bool bavgsent  = false;

  scale_payload.volt = readVcc();

  while (1) {
    unsigned long startmillis = millis();
    int16_t nWeight = gethx711(5);

    if (nWeight < 1500 || nofchecked > 120 || bavgsent ) {
      return;
    }

    ave.push(nWeight);
    if (( ave.stddev() < 20 ) && ( nofchecked > 8 ) && ( ave.mean() > 3000 ) && ( ave.mean() < 7000 ) && (!bavgsent)) {
      if (DEBUG_PRINT) {
        Serial.print("===> WeightAvg : ");
        Serial.print(ave.mean());
        Serial.print(" stddev ===> : ");
        Serial.println(ave.stddev());
        delay(10);
      } else {
        scale_payload.avemean   = (int16_t)ave.mean();
        scale_payload.avestddev = (int16_t)ave.stddev();
        scale_payload.avetype   = 1; // aveg
        radio.powerUp();
        radio.write(&scale_payload, sizeof(scale_payload));
        radio.powerDown();
      }
      bavgsent = true;
    }

    if ( nofchecked > 2 ) {
      if (DEBUG_PRINT) {
        Serial.print("===> WeightAvg : ");
        Serial.print(ave.mean());
        Serial.print(" stddev ===> : ");
        Serial.print(ave.stddev());
        Serial.print(" measured ===> : ");
        Serial.println(nWeight);
        delay(10);
      } else {
        if ( (nofchecked % 3) == 0 ) {
          scale_payload.avemean   = nWeight;
          scale_payload.avestddev = (int16_t)ave.stddev();
          scale_payload.avetype   = 0; // every
          radio.powerUp();
          radio.write(&scale_payload, sizeof(scale_payload));
          radio.powerDown();
        }
      }
    }
    ledonoff(2, 2);
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
    nofchecked++;
  }
}

void ledonoff(int m, int n) {
  for (int k = 0; k < m; k = k + 1) {
    if (k % 2 == 0) {
      digitalWrite(LED, HIGH);
    }
    else {
      digitalWrite(LED, LOW);
    }
    delay(n);
  }
  digitalWrite(LED, LOW);
}

void tarehx711() {
  int16_t nWeight = gethx711(2);
  scale_payload.avemean   = nWeight;
  scale_payload.avestddev = 0;
  scale_payload.avetype   = 4; // tare
  radio.powerUp();
  radio.write(&scale_payload, sizeof(scale_payload));
  radio.powerDown();

  scale.power_up();
  scale.set_scale(23500.f);
  scale.tare(10);
  scale.power_down();
}

int16_t gethx711(int m) {
  scale.power_up();
  float fmeasured = scale.get_units(m);
  scale.power_down();
  return (int16_t)(fmeasured * 1000) ;
}

void goingSleep() {
  if (DEBUG_PRINT) {
    Serial.println("01 ---> going to sleep");
  }
  ledonoff(2, 1);

  detachInterrupt(1);
  if (digitalRead(PIR_INT)) {
    LowPower.powerDown(SLEEP_500MS, ADC_OFF, BOD_OFF);
  } else {
    sleepXminutes(60);
    if (!bpir_isr) {
      tarehx711();
    }
  }
}

void sleepXminutes(int m) {
  int x = ( m * 60 ) / 8;
  bpir_isr = false;
  attachInterrupt(1, pir_isr, RISING);
  for (int i = 0; i < x; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  detachInterrupt(1);
  scale_payload.volt = readVcc();
}

int readVcc() {
  adc_enable();
  //ADMUX = _BV(MUX3) | _BV(MUX2);

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#endif

  delay(2);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1126400L / result; // Calculate Vcc (in mV);
  //result = 1074835L / result;

  //Disable ADC
  adc_disable();

  return (int)result; // Vcc in millivolts
}

// END

