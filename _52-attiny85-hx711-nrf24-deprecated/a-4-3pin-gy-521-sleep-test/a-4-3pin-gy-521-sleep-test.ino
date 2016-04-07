#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
// https://github.com/adafruit/TinyWireM
#include "USI_TWI_Master.h"
#include <TinyWireM.h>
#include "mpu6050.h"


#include <SoftwareSerial.h>
const int rx = -1;
const int tx = 1;

SoftwareSerial mySerial(rx, tx);

int16_t accelCount[3];

void setup() {
  pinMode(rx, INPUT);
  pinMode(tx, OUTPUT);
  mySerial.begin(9600);

  TinyWireM.begin();
  delay(100);
  Lowaccel();
}

void loop() {
}

void Lowaccel() {
  writeBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CLKSEL_BIT, MPU6050_PWR1_CLKSEL_LENGTH, MPU6050_CLOCK_PLL_XGYRO);
  writeBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_GYRO_CONFIG, MPU6050_GCONFIG_FS_SEL_BIT, MPU6050_GCONFIG_FS_SEL_LENGTH, MPU6050_GYRO_FS_250);
  writeBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_AFS_SEL_BIT, MPU6050_ACONFIG_AFS_SEL_LENGTH, MPU6050_ACCEL_FS_2);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_SLEEP_BIT, false);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_ENABLE, MPU6050_INTERRUPT_MOT_BIT, 1);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_ENABLE, MPU6050_INTERRUPT_FF_BIT, 1);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_ENABLE, MPU6050_INTERRUPT_ZMOT_BIT, 0);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_ENABLE, MPU6050_INTERRUPT_FIFO_OFLOW_BIT, 0);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_ENABLE, MPU6050_INTERRUPT_DATA_RDY_BIT, 0);
  writeBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_ACCEL_CONFIG, MPU6050_ACONFIG_ACCEL_HPF_BIT, MPU6050_ACONFIG_ACCEL_HPF_LENGTH, 3);
  writeBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_CONFIG, MPU6050_CFG_DLPF_CFG_BIT, MPU6050_CFG_DLPF_CFG_LENGTH, 5);
  writeByte(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_MOT_THR, 3);
  writeByte(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_MOT_DUR, 5);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_INT_LEVEL_BIT, 1);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_INT_OPEN_BIT, 0);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_LATCH_INT_EN_BIT, 0);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_INT_PIN_CFG, MPU6050_INTCFG_INT_RD_CLEAR_BIT, 0);
  writeByte(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_SMPLRT_DIV, 7);
  writeBits(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_2, MPU6050_PWR2_LP_WAKE_CTRL_BIT, MPU6050_PWR2_LP_WAKE_CTRL_LENGTH, 3);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_CYCLE_BIT, 1);
  delay(50);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_1, MPU6050_PWR1_TEMP_DIS_BIT, 1);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_2, MPU6050_PWR2_STBY_XG_BIT, 1);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_2, MPU6050_PWR2_STBY_YG_BIT, 1);
  writeBit(MPU6050_DEFAULT_ADDRESS, MPU6050_RA_PWR_MGMT_2, MPU6050_PWR2_STBY_ZG_BIT, 1);
  delay(50);
}


// i2c read and write from i2cdev //
bool writeBit(uint8_t devAddr, uint8_t regAddr, uint8_t bitNum, uint8_t data) {
  uint8_t b;
  readByte(devAddr, regAddr, &b, 1000);
  b = (data != 0) ? (b | (1 << bitNum)) : (b & ~(1 << bitNum));
  return writeByte(devAddr, regAddr, b);
}

bool writeBits(uint8_t devAddr, uint8_t regAddr, uint8_t bitStart, uint8_t length, uint8_t data) {
  uint8_t b;
  if (readByte(devAddr, regAddr, &b, 1000) != 0) {
    uint8_t mask = ((1 << length) - 1) << (bitStart - length + 1);
    data <<= (bitStart - length + 1); // shift data into correct position
    data &= mask; // zero all non-important bits in data
    b &= ~(mask); // zero all important bits in existing byte
    b |= data; // combine data with existing byte
    return writeByte(devAddr, regAddr, b);
  } else {
    return false;
  }
}

bool writeByte(uint8_t devAddr, uint8_t regAddr, uint8_t data) {
  return writeBytes(devAddr, regAddr, 1, &data);
}

bool writeBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t* data) {
  uint8_t status = 0;
  TinyWireM.beginTransmission(devAddr);
  TinyWireM.write((uint8_t) regAddr); // send address

  for (uint8_t i = 0; i < length; i++) {
    TinyWireM.write((uint8_t) data[i]);
  }
  status = TinyWireM.endTransmission();
  return status == 0;
}


int8_t readByte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint16_t timeout) {
  return readBytes(devAddr, regAddr, 1, data, timeout);
}

int8_t readBytes(uint8_t devAddr, uint8_t regAddr, uint8_t length, uint8_t *data, uint16_t timeout) {
  int8_t count = 0;
  uint32_t t1 = millis();
  for (uint8_t k = 0; k < length; k += min(length, USI_BUF_SIZE)) {
    TinyWireM.beginTransmission(devAddr);
    TinyWireM.write(regAddr);
    TinyWireM.endTransmission();
    TinyWireM.beginTransmission(devAddr);
    TinyWireM.requestFrom(devAddr, (uint8_t)min(length - k, USI_BUF_SIZE));

    for (; TinyWireM.available() && (timeout == 0 || millis() - t1 < timeout); count++) {
      data[count] = TinyWireM.read();
    }
    TinyWireM.endTransmission();
  }
  if (timeout > 0 && millis() - t1 >= timeout && count < length) count = -1; // timeout
  return count;
}

