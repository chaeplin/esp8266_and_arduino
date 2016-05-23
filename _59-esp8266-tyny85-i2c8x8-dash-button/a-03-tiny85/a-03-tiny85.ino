#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include "PinChangeInterrupt.h"
#include <TinyWireS.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define I2C_SLAVE_ADDR  0x26            // i2c slave address (38, 0x26)

#define ESP_RESET_PIN 3 // OUTPUT, PIN2
#define VCC_8x8       4 // OUTPUT, PIN3
#define BUTTON_PIN    1 // INPUT, PIN6

#  define DDR_USI             DDRB
#  define PORT_USI            PORTB
#  define PIN_USI             PINB
#  define PORT_USI_SDA        PB0
#  define PORT_USI_SCL        PB2
#  define PIN_USI_SDA         PINB0
#  define PIN_USI_SCL         PINB2
#  define USI_START_COND_INT  USISIF
#  define USI_START_VECTOR    USI_START_vect
#  define USI_OVERFLOW_VECTOR USI_OVF_vect

volatile int count   = 0;
volatile int command = 0;

unsigned int startMiils;

void receiveEvent(uint8_t num_bytes) {
  command = TinyWireS.receive();
}

void setup() {
  adc_disable();

  startMiils = millis();

  pinMode(ESP_RESET_PIN, OUTPUT);
  pinMode(VCC_8x8, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  TinyWireS.begin(I2C_SLAVE_ADDR);
  TinyWireS.onReceive(receiveEvent);
  //TinyWireS.onRequest(requestEvent);
}

void buttonCheck(void) {
  count++;
}

void pinint_sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  attachPCINT(digitalPinToPCINT(BUTTON_PIN), buttonCheck, FALLING);
  sleep_enable();
  sei();
  sleep_cpu();

  sleep_disable();
  sei();
  detachPCINT(digitalPinToPCINT(BUTTON_PIN));
  startMiils = millis();
}

void turn_off_8x8() {
  digitalWrite(VCC_8x8, LOW);
}

void turn_on_8x8() {
  digitalWrite(VCC_8x8, HIGH); 
}

void reset_esp() {
  digitalWrite(ESP_RESET_PIN, HIGH);
  delay(10);
  digitalWrite(ESP_RESET_PIN, LOW);  
  delay(10);
  digitalWrite(ESP_RESET_PIN, HIGH);
  delay(10);
  digitalWrite(ESP_RESET_PIN, LOW);
}

void loop() {
  if ( count == 0 ) {
    pinint_sleep();
  }

  if ( count == 1 ) {
    turn_on_8x8();
    reset_esp();
    count++;
  }

  while (command == 1) {
    turn_off_8x8();
    count   = 0;
    command = 0;
  }

  if ((millis() - startMiils) > 10000) {
    turn_off_8x8();
    count   = 0;
    command = 0;
  }
  
  TinyWireS_stop_check();
}
