//INCLUDES
#include <LowPower.h>
#include <Wire.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <I2Cdev.h> 
#include <MPU6050.h>

/*
  A2/16 --> VCC of gy-521
  A3/17 --> LED
*/

// PINS
#define LED       17
#define GY521_VCC 16

MPU6050 accelgyro;
unsigned long int stepcount = 0;
/*
  unsigned int detcdur = 5; //6
  unsigned int thrs = 3;
*/
unsigned int detcdur = 3; //6
unsigned int thrs    = 1;

boolean interruptA = false;

void wakeUpMotion() {
  disable_gy521();
  stepcount++;
  interruptA = true;
}

void wakeUpMotion_start() {
  detachInterrupt(0);
  attachInterrupt(0, wakeUpMotion, FALLING);
}

void setup() {
  Wire.begin();
  delay(100);
  pinMode(GY521_VCC, OUTPUT);
  enable_gy521();
  
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting.....");
  
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
  attachInterrupt(0, wakeUpMotion_start, RISING);
}

void loop() {
  if (interruptA) {
    interruptA = false;
    Serial.print("Steps: ");
    Serial.println(stepcount);
    enable_gy521();
  }
}

