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

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CE_PIN  3
#define CSN_PIN 3

#define DEVICE_ID 35
#define CHANNEL 100

const uint64_t pipes[1] = { 0xFFFFFFFFDDLL };
const int MPU_addr = 0x68;

byte buffer[14];

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

  delay(100);
  USICR =  (1 << USIWM1) | (0 << USIWM0);
  TinyWireM.begin();
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.write(0x6B);
  TinyWireM.write(0);
  TinyWireM.endTransmission();
}

void loop() {
  delay(1000);
  sendMPU_data();
  delay(200);
  getMPU_data();

}

void sendMPU_data() {
  USICR =  (1 << USIWM0) | (1 << USICS1) | (1 << USICLK) | (1 << USITC);
  SPI.begin();
  radio.write(&data_accelgyro , sizeof(data_accelgyro));
  
}


void getMPU_data() {
  /*
  PORT_USI |= (1<<PIN_USI_SDA);           // Enable pullup on SDA, to set high as released state.
  PORT_USI |= (1<<PIN_USI_SCL);           // Enable pullup on SCL, to set high as released state.
  
  DDR_USI  |= (1<<PIN_USI_SCL);           // Enable SCL as output.
  DDR_USI  |= (1<<PIN_USI_SDA);           // Enable SDA as output.
  USIDR    =  0xFF;                       // Preload dataregister with "released level" data.
  USICR    =  (0<<USISIE)|(0<<USIOIE)|                            // Disable Interrupts.
              (1<<USIWM1)|(0<<USIWM0)|                            // Set USI in Two-wire mode.
              (1<<USICS1)|(0<<USICS0)|(1<<USICLK)|                // Software stobe as counter clock source
              (0<<USITC);
  USISR   =   (1<<USISIF)|(1<<USIOIF)|(1<<USIPF)|(1<<USIDC)|      // Clear flags,
              (0x0<<USICNT0);                                     // and reset counter.
  */
  USICR =  (1 << USIWM1)|(0 << USIWM0);
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.write(0x3B);
  TinyWireM.endTransmission();

  delay(100);
  TinyWireM.requestFrom(MPU_addr, 14);

  DDR_USI  |= (0<<PIN_USI_SDA);           // Enable SDA as output.
  int i = 0;
  while (TinyWireM.available())
  {
    buffer[i] = TinyWireM.read();
    i++;
  }

  data_accelgyro.ax = (((int16_t)buffer[0]) << 8) | buffer[1];
  data_accelgyro.ay = (((int16_t)buffer[2]) << 8) | buffer[3];
  data_accelgyro.az = (((int16_t)buffer[4]) << 8) | buffer[5];
  data_accelgyro.gx = (((int16_t)buffer[8]) << 8) | buffer[9];
  data_accelgyro.gy = (((int16_t)buffer[10]) << 8) | buffer[11];
  data_accelgyro.gz = (((int16_t)buffer[12]) << 8) | buffer[13];
  
}

