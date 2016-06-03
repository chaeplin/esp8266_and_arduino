#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include "PinChangeInterrupt.h"
#include <TinyWireS.h>
// The default buffer size, Can't recall the scope of defines right now
//#ifndef TWI_RX_BUFFER_SIZE
//#define TWI_RX_BUFFER_SIZE ( 16 )
//#endif

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

volatile uint8_t count   = 0;
volatile uint8_t command = 0;

unsigned int startMiils;

void receiveEvent(uint8_t num_bytes) {
  command = TinyWireS.receive();
}

void requestEvent() {
  uint8_t x;
  if (count <= 2 ) {
    x = 1;
  }
  if (count == 3) {
    x = 2;  
  }
  if (count == 4) {
    x = 3;
  }  
  if (count > 4) {
    x = 4;       
  }
  
  TinyWireS.send(x);
}

void setup() {
  adc_disable();

  startMiils = millis();

  pinMode(ESP_RESET_PIN, OUTPUT);
  pinMode(VCC_8x8, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(ESP_RESET_PIN, HIGH);

  TinyWireS.begin(I2C_SLAVE_ADDR);
  TinyWireS.onReceive(receiveEvent);
  TinyWireS.onRequest(requestEvent);
}

void buttonCheck(void) {
  noInterrupts();
  count++;
  interrupts();
}

void pinint_sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  attachPCINT(digitalPinToPCINT(BUTTON_PIN), buttonCheck, FALLING);
  sleep_enable();
  sei();
  sleep_cpu();

  sleep_disable();
  sei();
}

void turn_off_8x8() {
  digitalWrite(VCC_8x8, LOW);
}

void turn_on_8x8() {
  digitalWrite(VCC_8x8, HIGH);
}

void reset_esp() {
  digitalWrite(ESP_RESET_PIN, LOW);
  tws_delay(10);
  digitalWrite(ESP_RESET_PIN, HIGH);
  /*
  tws_delay(10);
  digitalWrite(ESP_RESET_PIN, HIGH);
  tws_delay(10);
  digitalWrite(ESP_RESET_PIN, LOW);
  */
}

void loop() {
  if ( count == 0 ) {
    pinint_sleep();
  }

  if ( count == 1 ) {
    count++;
    turn_on_8x8();
    reset_esp();
  }

  if (command == 9 ) {
    turn_off_8x8();
    count   = 0;
    command = 0;
  }

  if ((millis() - startMiils) > 15000) {
    turn_off_8x8();
    count   = 0;
    command = 0;
  }

  TinyWireS_stop_check();
}
