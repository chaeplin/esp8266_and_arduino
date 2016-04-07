#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
// https://github.com/adafruit/TinyWireM
#include "USI_TWI_Master.h"
#include <TinyWireM.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#include <SoftwareSerial.h>
const int rx = -1;
const int tx = 1;

const int MPU_addr = 0x68;

struct {
  int16_t ax;
  int16_t ay;
  int16_t az;
  int16_t gx;
  int16_t gy;
  int16_t gz;
} data_accelgyro;

SoftwareSerial mySerial(rx, tx);

void setup() {
  pinMode(rx, INPUT);
  pinMode(tx, OUTPUT);
  mySerial.begin(9600);
  
  mySerial.println("gyro test");
    
  data_accelgyro.ax = 0;
  data_accelgyro.ay = 0;
  data_accelgyro.az = 0;
  data_accelgyro.gx = 0;
  data_accelgyro.gy = 0;
  data_accelgyro.gz = 0;

  delay(100);

  USICR =  (1 << USIWM1) | (0 << USIWM0);
  TinyWireM.begin();
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.write(0x6B);
  TinyWireM.write(0);
  TinyWireM.endTransmission();

  delay(100);
}

void loop() {
  getMPU_data();
  mySerial.print(data_accelgyro.ax);
  mySerial.print("\t");
  mySerial.print(data_accelgyro.ay);
  mySerial.print("\t");
  mySerial.print(data_accelgyro.az);
  mySerial.print("\t");
  mySerial.print(data_accelgyro.gx);
  mySerial.print("\t");
  mySerial.print(data_accelgyro.gy);
  mySerial.print("\t");
  mySerial.println(data_accelgyro.gz);
  delay(500);
}

void getMPU_data() {
  byte buffer[14];
  USICR =  (1 << USIWM1) | (0 << USIWM0);
  TinyWireM.beginTransmission(MPU_addr);
  TinyWireM.write(0x3B);
  TinyWireM.endTransmission();
  
  //delay(100);
  TinyWireM.requestFrom(MPU_addr, 14);
  
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
