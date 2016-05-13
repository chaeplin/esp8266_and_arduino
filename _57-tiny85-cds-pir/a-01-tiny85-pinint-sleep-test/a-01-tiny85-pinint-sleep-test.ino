#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CDS_PIN     A1 // P7
#define TR_BASE_PIN 1  // P6
#define INT_PIN     0  // P5

#include <SoftwareSerial.h>
const int rx = 3;
const int tx = 4;

SoftwareSerial mySerial(rx, tx);

volatile bool bpir_isr = false;
int lampon = false;

void setup() {

  pinMode(rx, INPUT);
  pinMode(tx, OUTPUT);
  mySerial.begin(9600);

  mySerial.println("cds test");
  mySerial.flush();

  pinMode(TR_BASE_PIN, OUTPUT);
  pinMode(INT_PIN, INPUT);

  if ( read_cds() < 50 ) {
    toggle_lamp();
  }

  delay(2000);
  if ( read_cds() >= 90 ) {
    lampon = true;
  } else {
    lampon = false;
  }
}

void loop() {
  mySerial.println("01 ----> going to sleep");

  if ( read_cds() >= 90 ) {
    lampon = true;
  } else {
    lampon = false;
  }
  
  timer_sleep();

  if (bpir_isr && !lampon) {
    mySerial.println("02 ----> wake up : isr and lamp is off");
    if ( read_cds() < 50 ) {
      toggle_lamp();
    }
    bpir_isr = false;
  } else if (!bpir_isr && lampon ) {
    mySerial.println("02 ----> wake up : no isr and lamp is on");
    toggle_lamp();
  }
}

void toggle_lamp() {
  mySerial.println("----> toggle lamp");
  digitalWrite(TR_BASE_PIN, HIGH);
  delay(30);
  digitalWrite(TR_BASE_PIN, LOW);
}

int read_cds() {

  delay(500);
  adc_enable();
  int VCC = readVcc();
  int val = analogRead(A1);
  adc_disable();
  val = map(val, 0, 1023, 0, VCC);
  val = map(val, 0, VCC, 0, 100);
  return val;

  /*
    adc_enable();
    int VCC = readVcc();
    mySerial.print("Vcc : ");
    mySerial.print(VCC);
    mySerial.print(" - cds : ");

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
    return val;
  */
}

void timer_sleep() {
  /*
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sei();
    sleep_cpu();

    cli();
    PCMSK &= ~_BV(PCINT0);
    sleep_disable();
    sei();
    bpir_isr = true;
  */

  while (digitalRead(INT_PIN)) {
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  }

  bpir_isr = false;

  for (int i = 0; i < 8; i++) {
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    
    PCMSK |= _BV(PCINT0);
    GIMSK |= _BV(PCIE);
    wdt_enable(9);
    WDTCR |= (1 << WDIE);

    sleep_enable();
    sei();
    sleep_cpu();

    cli();
    PCMSK &= ~_BV(PCINT0);
    sleep_disable();
    sei();    

    if (digitalRead(INT_PIN) ) {
      bpir_isr = true;
      break;
    }
  }
}


int readVcc() {
  ADMUX = _BV(MUX3) | _BV(MUX2);

  delay(2);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  //result = 1126400L / result; // Calculate Vcc (in mV);
  result = 1074835L / result;

  return (int)result; // Vcc in millivolts
}
