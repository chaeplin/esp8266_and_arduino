#include <LowPower.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/pgmspace.h>
#include <nRF24L01.h>
#include <RF24.h>

//Disabling ADC saves ~230uAF. Needs to be re-enable for the internal voltage check
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC
/*
  ADCSRA &= ~_BV(ADEN);                   // ADC off
  ADCSRA |= _BV(ADEN);                    // ADC on
*/

#define CE_PIN 5
#define CSN_PIN 4

#define DEVICE_ID 3
#define CHANNEL 1

const uint64_t pipes[2] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL };

typedef struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t temp;
  int16_t humi;
  uint8_t devid;
} data;

data payload;

RF24 radio(CE_PIN, CSN_PIN);

const int statusPin = 3;
volatile int statusPinStatus ;

void setup() {
  delay(20);
  unsigned long startmilis = millis();
  adc_disable();

  pinMode(statusPin, INPUT_PULLUP);
  statusPinStatus = digitalRead(statusPin);

  radio.begin();
  radio.setRetries(15, 15);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(11);
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();

  unsigned long stopmilis = millis();
  payload.humi = ( stopmilis - startmilis ) * 10 ;

  payload._salt = 0;
  payload.devid = DEVICE_ID;
}

void loop() {
  sleep();

  statusPinStatus = digitalRead(statusPin);
  if ( statusPinStatus == HIGH) {

    payload._salt++;
    unsigned long startmilis = millis();

    payload.temp =  10 ;
    payload.volt = readVcc();

    if ( payload.volt > 5000 || payload.volt <= 0 ) {
      return;
    }

    radio.powerUp();
    radio.write(&payload , sizeof(payload));
    radio.powerDown();

    unsigned long stopmilis = millis();
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

void sleep() {
  GIMSK |= _BV(PCIE);                     // Enable Pin Change Interrupts
  PCMSK |= _BV(PCINT3);                   // Use PB3 as interrupt pin
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // replaces above statement

  sleep_enable();                         // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
  sei();                                  // Enable interrupts
  sleep_cpu();                            // sleep

  cli();                                  // Disable interrupts
  PCMSK &= ~_BV(PCINT3);                  // Turn off PB3 as interrupt pin
  sleep_disable();                        // Clear SE bit
  //sei();                                  // Enable interrupts
}

ISR(PCINT0_vect) {
}

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
  result = 1074835L / result;

  //Disable ADC
  adc_disable();

  return (int)result; // Vcc in millivolts
}
