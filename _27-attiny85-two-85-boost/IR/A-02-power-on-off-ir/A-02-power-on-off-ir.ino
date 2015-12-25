#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>

//Disabling ADC saves ~230uAF. Needs to be re-enable for the internal voltage check
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CONVETER_PIN_OUT 4  // PIN 3 / D4
#define INT_PIN_IN       2  // PIN 7 / INT 0
#define VCC_PIN_OUT      3  // PIN 2 / D3


volatile int secon85status;
int o_secon85status;
int resetstatus;

unsigned long startMills = 0;

void setup() {

  startMills = millis();

  secon85status = o_secon85status = resetstatus = LOW;

  adc_disable();

  // enable converter // low active
  pinMode(CONVETER_PIN_OUT, OUTPUT);
  pinMode(VCC_PIN_OUT, OUTPUT);
  digitalWrite(CONVETER_PIN_OUT, HIGH);

  // INT input
  pinMode(INT_PIN_IN, INPUT);

}

void loop() {

  if ( resetstatus == LOW ) {
    powerOn();
    delay(100);
    removePoweronInt();
    resetstatus = HIGH;
  }

  if (( resetstatus == HIGH ) && ( secon85status != o_secon85status )) {
    delay(50);
    powerDn();
    o_secon85status = secon85status;
    resetstatus = LOW;
    sleep();
  }

  if ((millis() - startMills) > 5000) {
    delay(50);
    powerDn();
    o_secon85status = secon85status;
    resetstatus = LOW;
    sleep();
  }

}

void removePoweronInt() {
  attachInterrupt(0, intFromsecon85activate, RISING);
}

void intFromsecon85activate() {
  detachInterrupt(0);
  attachInterrupt(0, intFromsecon85, RISING);
}

void intFromsecon85() {
  secon85status = !secon85status;
  detachInterrupt(0);
}

void powerOn() {
  digitalWrite(CONVETER_PIN_OUT, LOW);
  digitalWrite(VCC_PIN_OUT, HIGH);
}

void powerDn() {
  digitalWrite(CONVETER_PIN_OUT, HIGH);
  digitalWrite(VCC_PIN_OUT, LOW);
}

void sleep() {
  //for (int i = 0; i < 7; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  //}
  //LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
}
