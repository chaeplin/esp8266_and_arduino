#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
// https://github.com/adafruit/TinyWireM
#include "USI_TWI_Master.h"
#include <TinyWireM.h>
//
#include <nRF24L01.h>
#include <RF24.h>
//
// https://github.com/bogde/HX711
#include "HX711.h"

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// 3 pin version
#define CE_PIN  10
#define CSN_PIN 10

#define DEVICE_ID 35
#define CHANNEL 100

const uint64_t pipes[1] = { 0xFFFFFFFFDDLL };
const int MPU_addr = 0x68;

struct {
  int16_t ax;
  int16_t ay;
} data_scale;

HX711 scale(3, 4);
RF24 radio(CE_PIN, CSN_PIN);

void setup() {
  /*
  // set GY-521 to intterupt mode off
  // todo
  //USICR =  (1 << USIWM1) | (0 << USIWM0);
  TinyWireM.begin();
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.write(0x6B);
  TinyWireM.write(0);
  TinyWireM.endTransmission();
  USI_TWI_Master_Stop();
  */

  data_scale.ax = 0;
  data_scale.ay = 0;

  scale.set_scale(22.f);

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
  //radio.powerDown();

}

void loop() {
  scale.power_down();
  data_scale.ax = scale.get_units();
  data_scale.ay = scale.get_units(10);
  scale.power_up();
  
  send_data();
  delay(1000);
}

void send_data() {
  radio.write(&data_scale , sizeof(data_scale));
}

void set_int() {
  // set GY-521 to intterupt mode on
  // todo
  //USICR =  (1 << USIWM1) | (0 << USIWM0);
  TinyWireM.begin();
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.write(0x6B);
  TinyWireM.write(0);
  TinyWireM.endTransmission();
  USI_TWI_Master_Stop();  
}
