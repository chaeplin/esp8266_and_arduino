// IR code from https://github.com/A2K/wr703n-attiny85-infrared/blob/master/ATtiny85_I2C_IR/ATtiny85_I2C_IR.ino
// 
#include <avr/sleep.h>
#include <avr/interrupt.h>

namespace Codes
{

typedef uint32_t Code;

namespace Water
{
Code WaterLow  = 0xFF5A50AF;
Code WaterFull = 0xFF5AD02F;
}
}

namespace IR
{
class Sender
{
    uint8_t pins;

  public:
    Sender(uint8_t pins):
      pins(pins)
    {
    }

    void blink(unsigned long us)
    {
      for (size_t i = 0; i < (us / 26); i++)
      {
        PORTB |= pins;
        delayMicroseconds(9);
        PORTB &= ~pins;
        delayMicroseconds(9);
      }
    }

    void header()
    {
      blink(9000);
      delayMicroseconds(4500);
    }

    void one()
    {
      blink(562);
      delayMicroseconds(1686);
    }

    void zero()
    {
      blink(562);
      delayMicroseconds(562);
    }

    void send(Codes::Code code, unsigned int times = 2)
    {
      for (unsigned int i = 0; i < times; ++i)
      {
        actuallySend(code);
        delayMicroseconds(2250);
      }
    }

    void actuallySend(Codes::Code code)
    {
      header();

      for (unsigned int i = 0; i < 32; ++i, code <<= 1)
      {
        if (code & 0x80000000)
        {
          one();
        }
        else
        {
          zero();
        }
      }

      blink(562);
    }
};
}


const int switchPin                     = 3;
const int statusLED                     = 4;
const int irPin                         = 1;
volatile int switchPinStatus ;

IR::Sender sender(2);

void setup() {

  //pinMode(switchPin, INPUT);
  //digitalWrite(switchPin, HIGH);
  pinMode(switchPin, INPUT_PULLUP);
  pinMode(statusLED, OUTPUT);
  pinMode(irPin, OUTPUT);

  //switchPinStatus = LOW;
  switchPinStatus = digitalRead(switchPin);

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

ISR(PCINT0_vect) {
  switchPinStatus = digitalRead(switchPin);
}

void loop() {
  sleep();

  if ( switchPinStatus == HIGH) {
    digitalWrite(statusLED, HIGH);
    delay(1000);
    digitalWrite(statusLED, LOW);
    sender.send(Codes::Water::WaterLow);
  } else {
    digitalWrite(statusLED, HIGH);
    delay(500);
    digitalWrite(statusLED, LOW);
    sender.send(Codes::Water::WaterFull);
  }
}

