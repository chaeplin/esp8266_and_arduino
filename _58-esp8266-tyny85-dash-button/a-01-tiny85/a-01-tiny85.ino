#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// http://www.gammon.com.au/forum/?id=12769
#if defined(__AVR_ATtiny85__)
#define watchdogRegister WDTCR
#else
#define watchdogRegister WDTCSR
#endif

#define ESP_RESET_PIN 4
#define ESP_SIG_PIN   3
#define BUTTON_PIN    1
#define LED_PIN       0

void setup() {
  adc_disable();
  pinMode(ESP_RESET_PIN, OUTPUT);     // high
  pinMode(ESP_SIG_PIN, INPUT_PULLUP);       //
  pinMode(BUTTON_PIN, INPUT_PULLUP);  //

  pinMode(LED_PIN, OUTPUT);

  //digitalWrite(ESP_SIG_PIN, HIGH);
  digitalWrite(ESP_RESET_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);

}

void loop() {
  pinint_sleep();
  if (!digitalRead(BUTTON_PIN) && digitalRead(ESP_SIG_PIN)) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(ESP_RESET_PIN, LOW);
    delay(10);
    digitalWrite(ESP_RESET_PIN, HIGH);
    digitalWrite(LED_PIN, LOW);
  }
  goToSleep();
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
  PCMSK &= ~_BV(PCINT1);
  sleep_disable();
  sei();
}

void pinint_sleep() {
  PCMSK |= _BV(PCINT1);
  GIMSK |= _BV(PCIE);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sei();
  sleep_cpu();
}
