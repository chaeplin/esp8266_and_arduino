#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <nRF24L01.h>
#include <RF24.h>
// https://github.com/adafruit/TinyWireM
#include "USI_TWI_Master.h"
#include <TinyWireM.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CE_PIN  3
#define CSN_PIN 3

#define DEVICE_ID 35
#define CHANNEL 100

const uint64_t pipes[1] = { 0xFFFFFFFFDDLL };
const int MPU_addr = 0x68;

struct {
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
} data_accelgyro;

RF24 radio(CE_PIN, CSN_PIN);

void setup() {

  data_accelgyro.ax = 0;
  data_accelgyro.ay = 0;
  data_accelgyro.az = 0;
  data_accelgyro.gx = 0;
  data_accelgyro.gy = 0;
  data_accelgyro.gz = 0;

  //
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

  TinyWireM.begin();
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.send(0x6B);
  TinyWireM.send(0);
  TinyWireM.endTransmission(true);
}

void loop() {
  getMPU_data();
  sendMPU_data();
  
  delay(1000);
}

void sendMPU_data() {
  USICR =  (1 << USIWM0) | (1 << USICS1) | (1 << USICLK) | (1 << USITC);
  SPI.begin();
  radio.powerUp();
  radio.write(&data_accelgyro , sizeof(data_accelgyro));
  radio.powerDown();
}

void getMPU_data() {
  USICR =  (1 << USIWM1) | (0 << USIWM0);
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.send(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  TinyWireM.endTransmission(false);
  TinyWireM.requestFrom(MPU_addr, 14); // request a total of 14 registers

  data_accelgyro.ax = TinyWireM.receive() << 8 | TinyWireM.receive();
  data_accelgyro.ay = TinyWireM.receive() << 8 | TinyWireM.receive();
  data_accelgyro.az = TinyWireM.receive() << 8 | TinyWireM.receive();
  int16_t Tmp       = TinyWireM.receive() << 8 | TinyWireM.receive();
  data_accelgyro.gx = TinyWireM.receive() << 8 | TinyWireM.receive();
  data_accelgyro.gy = TinyWireM.receive() << 8 | TinyWireM.receive();
  data_accelgyro.gz = TinyWireM.receive() << 8 | TinyWireM.receive();
}
