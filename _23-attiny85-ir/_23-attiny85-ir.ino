#include <avr/sleep.h>
#include <avr/interrupt.h>

const int switchPin                     = 3;
const int statusLED                     = 2;

void setup() {

  pinMode(switchPin, INPUT);
  digitalWrite(switchPin, HIGH);
  pinMode(statusLED, OUTPUT);

  // Flash quick sequence so we know setup has started
  for (int k = 0; k < 10; k = k + 1) {
    if (k % 2 == 0) {
      digitalWrite(statusLED, HIGH);
    }
    else {
      digitalWrite(statusLED, LOW);
    }
    delay(250);
  } // for
} // setup

void sleep() {

  GIMSK |= _BV(PCIE);                     // Enable Pin Change Interrupts
  PCMSK |= _BV(PCINT3);                   // Use PB3 as interrupt pin
  ADCSRA &= ~_BV(ADEN);                   // ADC off
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // replaces above statement

  sleep_enable();                         // Sets the Sleep Enable bit in the MCUCR Register (SE BIT)
  sei();                                  // Enable interrupts
  sleep_cpu();                            // sleep

  cli();                                  // Disable interrupts
  PCMSK &= ~_BV(PCINT3);                  // Turn off PB3 as interrupt pin
  sleep_disable();                        // Clear SE bit
  //ADCSRA |= _BV(ADEN);                    // ADC on

  sei();                                  // Enable interrupts
} // sleep

ISR(PCINT3_vect) {
  // This is called when the interrupt occurs, but I don't need to do anything in it
}

void loop() {
  sleep();
  digitalWrite(statusLED, HIGH);
  delay(1000);
  digitalWrite(statusLED, LOW);
}
