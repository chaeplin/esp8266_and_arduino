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
#include <avr/power.h>
#include <nRF24L01.h>
#include <RF24.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// http://www.gammon.com.au/forum/?id=12769
#if defined(__AVR_ATtiny85__)
#define watchdogRegister WDTCR
#else
#define watchdogRegister WDTCSR
#endif

#define _3PIN
//#define _5PIN

#ifdef _3PIN
// 3pin
#define CE_PIN 3
#define CSN_PIN 3
#else
// 5pin
#define CE_PIN 5
#define CSN_PIN 4
#endif

#define DEVICE_ID 65
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

#define IRENPIN 4 // p3 / A2
#define DATA1PIN 3 // p2 // A3

int16_t doorStatus;
int16_t rollStatus;

void setup() {
  delay(20);
  adc_disable();
  unsigned long startmilis = millis();

  pinMode(DATA1PIN, INPUT);
  pinMode(IRENPIN, OUTPUT);
  digitalWrite(IRENPIN, LOW);

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(15, 15);
  radio.enableDynamicPayloads();
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  radio.powerDown();

  unsigned long stopmilis = millis();
  payload.data2 = ( stopmilis - startmilis ) * 10 ;
  payload.data1  = rollStatus = digitalRead(DATA1PIN) * 10 ;
  payload._salt = 0;
  payload.volt = readVcc();
  payload.devid = DEVICE_ID;

  radio.powerUp();
  radio.write(&payload , sizeof(payload));
  radio.powerDown();
}

void loop() {
  pinint_sleep();
  goToSleep ();
  doorStatus = digitalRead(DATA1PIN);
  if (doorStatus == 1 ) {
    return;
  } else {
    unsigned long startmilis = millis();
    payload._salt++;
    payload.volt = readVcc();

    digitalWrite(IRENPIN, HIGH);
    delay(1);
    payload.data1 = digitalRead(DATA1PIN) * 10;
    digitalWrite(IRENPIN, LOW);
    
    //if (payload.data1 == 10) {
    if ( rollStatus != payload.data1 ) {
      radio.powerUp();
      radio.write(&payload , sizeof(payload));
      radio.powerDown();
      rollStatus = payload.data1;
    }
    unsigned long stopmilis = millis();
    payload.data2 = ( stopmilis - startmilis ) * 10 ;
  }
}

// http://www.gammon.com.au/forum/?id=12769
// watchdog interrupt
ISR (WDT_vect)
{
  wdt_disable();  // disable watchdog
}

void goToSleep ()
{
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);
  noInterrupts ();       // timed sequence coming up

  // pat the dog
  wdt_reset();

  // clear various "reset" flags
  MCUSR = 0;
  // allow changes, disable reset, clear existing interrupt
  watchdogRegister = bit (WDCE) | bit (WDE) | bit (WDIF);
  // set interrupt mode and an interval (WDE must be changed from 1 to 0 here)
  watchdogRegister = bit (WDIE) | bit (WDP2) | bit (WDP1) | bit (WDP0);    // set WDIE, and 2 seconds delay

  sleep_enable ();       // ready to sleep
  interrupts ();         // interrupts are required now
  sleep_cpu ();          // sleep
  sleep_disable ();      // precaution
}  // end of goToSleep

ISR(PCINT0_vect) {
  cli();
  PCMSK &= ~_BV(PCINT3);
  sleep_disable();
  sei();
}

void pinint_sleep() {
  PCMSK |= _BV(PCINT3);
  GIMSK |= _BV(PCIE);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
}

int readVcc() {
  adc_enable();
  ADMUX = _BV(MUX3) | _BV(MUX2);

  delay(2);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  //result = 1126400L / result; // Calculate Vcc (in mV);
  result = 1074835L / result;

  //Disable ADC
  adc_disable();

  return (int)result; // Vcc in millivolts
}
