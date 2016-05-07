#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#include <SoftwareSerial.h>
const int rx = 3;
const int tx = 4;

#define CDS_PIN A1

SoftwareSerial mySerial(rx, tx);

void setup() {
  adc_disable();

  pinMode(rx, INPUT);
  pinMode(tx, OUTPUT);
  mySerial.begin(9600);

  mySerial.println("cds test");
  mySerial.flush();
}

void loop() {
  int VCC = readVcc();
  mySerial.print("Vcc : ");
  mySerial.print(VCC);
  mySerial.print(" - cds : ");
  
  adc_enable();
  delay(2);
  int val = analogRead(A1);
  adc_disable();
  mySerial.print(val);
  val = map(val, 0, 1023, 0, VCC);
  mySerial.print(" - Vo : ");
  mySerial.print(val);
  val = map(val, 0, VCC, 0, 100);
  mySerial.print(" - % : ");
  mySerial.println(val);
  
  mySerial.flush();
  delay(2000);

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
