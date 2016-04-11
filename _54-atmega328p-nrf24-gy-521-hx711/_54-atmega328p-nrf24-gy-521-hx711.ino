// base is _02-mqtt-sw-temperature
// 5pin
/* 24l01p    pro mini/mega328
   1  gnd   -
   2  vcc   +
   3  ce    9
   4  csn   10
   5  sck   13
   6  mosi  11
   7  miso  12
  //-
  int 0 --> int of gy-521
  int 1 --> int of pir
  //
  A5/19 --> Wire SCL
  A4/18 --> Wire SDA
  A3/17 --> LED
  A2/16 --> VCC of gy-521
*/
#include <LowPower.h>
#include <TimeLib.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
//
#include "I2Cdev.h"
#include "MPU6050.h"
//
#include "HX711.h"

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// http://www.gammon.com.au/forum/?id=12769
#if defined(__AVR_ATtiny85__)
#define watchdogRegister WDTCR
#else
#define watchdogRegister WDTCSR
#endif

// PINS
#define LED       17
#define GY521_VCC 16
#define PIR_INT   3
#define GY521_INT 2

// nrf24
#define CE_PIN    9
#define CSN_PIN   10

// HX711
#define HX711_SCK 8
#define HX711_DT  7

// nrf24
#define DEVICE_ID 35
#define CHANNEL   100

const uint64_t pipes[1] = { 0xFFFFFFFFCDLL };

struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t data1;
  int16_t data2;
  uint8_t devid;
} payload;

RF24 radio(CE_PIN, CSN_PIN);
MPU6050 accelgyro;
HX711 scale(HX711_DT, HX711_SCK);

unsigned int detcdur = 3;
unsigned int thrs    = 1;

volatile bool bpir_isr;
volatile bool bmotion_isr;

void pir_isr() {
  bpir_isr = true;
}

void motion_isr() {
  disable_gy521();
  bmotion_isr = true;
}

void motion_isr_start() {
  detachInterrupt(0);
  attachInterrupt(0, motion_isr, FALLING);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("00 --> Starting unit : wire, gyro, pir, hx711, radio");

  Wire.begin();

  // radio
  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(15, 15);
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  radio.powerDown();

  payload._salt = 0;
  payload.volt  = readVcc();
  payload.devid = DEVICE_ID;

  bpir_isr = bmotion_isr = false;

  scale.set_scale(22852.f);
  scale.tare();
  scale.power_down();

  //
  pinMode(LED, OUTPUT);
  pinMode(GY521_VCC, OUTPUT);
  // INT
  pinMode(PIR_INT, INPUT); //_PULLUP);
  pinMode(GY521_INT, INPUT); //_PULLUP);
  //
}

void loop() {
  goingSleep();

  if (bpir_isr) {
    Serial.println("02 ---> pir detected --> going to timer sleep");

    //tarehx711();
    goingTimerSleep();

    if (bmotion_isr) {
      while (1) {
        unsigned long startmillis = millis();
        int16_t nWeight = gethx711();
        if (nWeight < 1000) {
          break;
        }
        LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
      }
    }

  }
  Serial.println("05 ---> start again");
  Serial.println();
}

void tarehx711() {
  scale.power_up();
  scale.set_scale(22852.f);
  scale.tare();
  scale.power_down();
}

int16_t gethx711() {
  scale.power_up();
  float fmeasured = scale.get_units(5);
  scale.power_down();
  return (int16_t)(fmeasured * 1000) ;
}

void goingSleep() {
  Serial.println("01 ---> going to sleep");
  delay(100);

  detachInterrupt(1);
  while (digitalRead(PIR_INT)) {
    LowPower.powerDown(SLEEP_250MS, ADC_OFF, BOD_OFF);
  }
  bpir_isr = bmotion_isr = false;
  attachInterrupt(1, pir_isr, RISING);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  detachInterrupt(1);
}

void goingTimerSleep() {
  Serial.println("03 ---> going to timer sleep");
  delay(100);

  enable_gy521();
  LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  detachInterrupt(0);
  digitalWrite(LED, LOW);
}

void disable_gy521() {
  detachInterrupt(0);
  digitalWrite(GY521_VCC, LOW);
}

void enable_gy521() {
  digitalWrite(GY521_VCC, HIGH);
  delay(100);

  accelgyro.initialize();
  Wire.beginTransmission(0x68);
  Wire.write(0x37);
  Wire.write(0x02);
  Wire.endTransmission();

  Wire.beginTransmission(0x68);
  Wire.write(0x6A);
  Wire.write(0x00);
  Wire.endTransmission();

  //Disable Sleep Mode
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission();

  accelgyro.setIntMotionEnabled(1);//Set Motion Detection interrupt enabled status.
  accelgyro.setIntFreefallEnabled(0);
  accelgyro.setIntZeroMotionEnabled(0);
  accelgyro.setIntFIFOBufferOverflowEnabled(0);
  accelgyro.setIntDataReadyEnabled(0); //New interrupt enabled status
  accelgyro.setDHPFMode(3); //New high-pass filter configuration more than 1.25Hz pass
  accelgyro.setDLPFMode(5); //New low-pass filter configuration below 10Hz pass
  accelgyro.setMotionDetectionThreshold(thrs);  //20 - 2
  accelgyro.setMotionDetectionDuration(detcdur); //New motion detection duration threshold value (LSB = 1ms)
  accelgyro.setInterruptMode(1); //New interrupt mode (0=active-high, 1=active-low)
  accelgyro.setInterruptDrive(0); //New interrupt drive mode (0=push-pull, 1=open-drain)
  accelgyro.setInterruptLatch(0); //New latch mode (0=50us-pulse, 1=latch-until-int-cleared)
  accelgyro.setInterruptLatchClear(0); //New latch clear mode (0=status-read-only, 1=any-register-read)
  accelgyro.setRate(7); //Set the rate to disable the gyroscope

  delay(50);
  accelgyro.setTempSensorEnabled(0);
  accelgyro.setStandbyXGyroEnabled(1);
  accelgyro.setStandbyYGyroEnabled(1);
  accelgyro.setStandbyZGyroEnabled(1);
  accelgyro.setStandbyXAccelEnabled(0);
  accelgyro.setStandbyYAccelEnabled(0);
  accelgyro.setStandbyZAccelEnabled(0);
  delay(100);
  attachInterrupt(0, motion_isr_start, RISING);
}

int readVcc() {
  adc_enable();
  //ADMUX = _BV(MUX3) | _BV(MUX2);

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#endif

  delay(2);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1126400L / result; // Calculate Vcc (in mV);
  //result = 1074835L / result;

  //Disable ADC
  adc_disable();

  return (int)result; // Vcc in millivolts
}
// END

