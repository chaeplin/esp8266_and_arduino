// 5pin
/* 24l01    85
   1  gnd   4
   2  vcc   8
   3  ce    1
   4  csn   3
   5  sck   7
   6  mosi  6
   7  miso  5
*/

// 3pin
/* 24l01    85
   1  gnd   4
   2  vcc   8
   3  ce    x
   4  csn   x
   5  sck   7
   6  mosi  6
   7  miso  5
*/

#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <Average.h>
#include <nRF24L01.h>
#include <RF24.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define _3PIN
//#define _5PIN

#ifdef _3PIN
// 3pin
#define CE_PIN 7
#define CSN_PIN 7
#else
// 5pin
#define CE_PIN 5
#define CSN_PIN 4
#endif

#define DEVICE_ID 55
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

#define DATA1PIN A2  // A2 // PIN 3 // D 4
#define VCCPIN A3  //  A3 // PIN 2 // D 3

Average<float> ave(2);

void setup() {
  //adc_disable();
  unsigned long startmilis = millis();

  delay(100);

  payload.devid = DEVICE_ID;
  payload._salt = 10;

  //delay(500);

  payload.volt = readVolt();
  readData1();
  /*
  payload.data1 = readData1() * 10;

  unsigned long stopmilis = millis();
  //payload.data2 = ( stopmilis - startmilis ) * 10 ;
  */

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  radio.setPayloadSize(11);
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  //radio.powerDown();

  //radio.powerUp();
  radio.write(&payload , sizeof(payload));
  radio.powerDown();

}

void loop() {
  //sleep();
}

void sleep() {
  adc_disable();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // replaces above statement
  sleep_enable();                         // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
  sleep_cpu();                            // sleep
}

/*
int readVcc() {
  adc_enable();
  ADMUX = _BV(MUX3) | _BV(MUX2);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  //result = 1126400L / result; // Calculate Vcc (in mV);
  //result = 1074835L / result;
  result =    866991L / result ;

  //Disable ADC
  adc_disable();

  return (int)result; // Vcc in millivolts
}

int16_t readData1() {
  for (int k = 0; k < 10; k = k + 1) {
    uint16_t value = ultrasonic.Ranging(CM);
    ave.push(value);
    delay(50);
  }

  return ave.mean() ;

}
*/


uint16_t readVolt() {
  for (int k = 0; k < 2; k = k + 1) {
    uint16_t value = analogRead(VCCPIN);
    ave.push(value);
    delay(5);
  }
  return ((ave.mean() * (5.0 / 1023.0)) * 1000 );
}

void readData1() {
  for (int k = 0; k < 2; k = k + 1) {
    uint16_t value = analogRead(DATA1PIN);
    ave.push(value);
    delay(5);
  }
  if (ave.mean() < 10) {
    payload.data1 =  ((( 67870.0 / (10 - 3.0)) - 40.0) * 10 ) ;
  } else {
    payload.data1 =  ((( 67870.0 / (ave.mean() - 3.0)) - 40.0) * 10) ;
  }
  payload.data2 = ave.stddev() * 10 ;
}
