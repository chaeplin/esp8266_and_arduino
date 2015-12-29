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
#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <Average.h>
#include <nRF24L01.h>
#include <RF24.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

//#define _TEST
#define _DIGITAL
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
#define DATA1PIN A3 // p2

Average<float> ave(2);

void setup() {
  delay(20);
  adc_disable();
  unsigned long startmilis = millis();

#ifdef _DIGITAL
  pinMode(DATA1PIN, INPUT_PULLUP);
#endif
  pinMode(IRENPIN, OUTPUT);
  digitalWrite(IRENPIN, LOW);

  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  radio.setPayloadSize(11);
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  //radio.powerDown();

  unsigned long stopmilis = millis();
  payload.data2 = ( stopmilis - startmilis ) * 10 ;

  payload._salt = 0;
  payload.devid = DEVICE_ID;

}

void loop() {
  unsigned long startmilis = millis();
  payload._salt++;
  digitalWrite(IRENPIN, HIGH);
  payload.volt = readVcc();
#ifdef _DIGITAL
  //delay(3);
  payload.data1  = digitalRead(DATA1PIN) * 10 ;
#else
  //delay(3);
  payload.data1  = readData1() * 10;
#endif
  digitalWrite(IRENPIN, LOW);

  radio.powerUp();
  radio.write(&payload , sizeof(payload));
  radio.powerDown();

  unsigned long stopmilis = millis();

  payload.data2 = ( stopmilis - startmilis ) * 10 ;

  if ((millis() - startmilis) > 1000) {
    sleep();
    return;
  }

  sleep();

}

void sleep() {
#ifdef _TEST
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
#else
  for (int i = 0; i < 7; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
#endif
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

int16_t readData1() {

  adc_enable();
  delay(2);
  for (int k = 0; k < 2; k = k + 1) {
    uint16_t value = analogRead(DATA1PIN);
    ave.push(value);
    delay(5);
  }
  return ave.mean();
  adc_disable();
}
