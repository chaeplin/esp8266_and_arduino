//INCLUDES
#include <LowPower.h>
#include <Wire.h>
#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <I2Cdev.h> //nothing to do 
#include <MPU6050.h>

MPU6050 accelgyro;
unsigned long int stepcount = 0;
/*
unsigned int detcdur = 5; //6
unsigned int thrs = 3;
*/
unsigned int detcdur = 3; //6
unsigned int thrs = 1;

boolean interruptA = false;

void wakeUpMotion() {
  if (interruptA == 0) {
    stepcount++;
    interruptA = true;
  }
}

void setup() {
  startsensors();
  attachInterrupt(0, wakeUpMotion, LOW);
  Serial.begin(115200);
  delay(100);
}

void startsensors() {
  Wire.begin();
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
  accelgyro.setIntFreefallEnabled(1);
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
  accelgyro.setWakeFrequency(3); //Wake up the accelerometer at 1.25Hz to save power
  accelgyro.setWakeCycleEnabled(1); //Enable only accel. ON - Low power mode, waking it up from time to time

  /*          |   ACCELEROMETER    |           GYROSCOPE
    DLPF_CFG | Bandwidth | Delay  | Bandwidth | Delay  | Sample Rate
    ---------+-----------+--------+-----------+--------+-------------
    0        | 260Hz     | 0ms    | 256Hz     | 0.98ms | 8kHz
    1        | 184Hz     | 2.0ms  | 188Hz     | 1.9ms  | 1kHz
    2        | 94Hz      | 3.0ms  | 98Hz      | 2.8ms  | 1kHz
    3        | 44Hz      | 4.9ms  | 42Hz      | 4.8ms  | 1kHz
    4        | 21Hz      | 8.5ms  | 20Hz      | 8.3ms  | 1kHz
    5        | 10Hz      | 13.8ms | 10Hz      | 13.4ms | 1kHz
    6        | 5Hz       | 19.0ms | 5Hz       | 18.6ms | 1kHz
    ACCEL_HPF | Filter Mode | Cut-off Frequency
    ----------+-------------+------------------
    0         | Reset       | None
    1         | On          | 5Hz
    2         | On          | 2.5Hz
    3         | On          | 1.25Hz
    4         | On          | 0.63Hz
    7         | Hold        | None
    LP_WAKE_CTRL | Wake-up Frequency
    -------------+------------------
    0            | 1.25 Hz
    1            | 2.5 Hz
    2            | 5 Hz
    3            | 10 H*/

  delay(50);
  accelgyro.setTempSensorEnabled(0);
  accelgyro.setStandbyXGyroEnabled(1);
  accelgyro.setStandbyYGyroEnabled(1);
  accelgyro.setStandbyZGyroEnabled(1);
  delay(50);
}

void loop() {
  if (interruptA) {
    interruptA = false;
    Serial.print("Steps: ");
    Serial.println(stepcount);
  }
  delay(10);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  delay(10);
}

