#include <avr/power.h>
#include <avr/sleep.h>

#include <Wire.h>
#include <MMA8452.h>

MMA8452 accelerometer;

volatile bool interrupt;
volatile bool sleeping;

void setup()
{
  Serial.begin(115200);
  Serial.print(F("Initializing MMA8452Q: "));

  Wire.begin();

  pinMode(5, OUTPUT);
  pinMode(6, OUTPUT);

  bool initialized = accelerometer.init();

  if (initialized)
  {
    Serial.println(F("ok"));

    accelerometer.setPowerMode(MMA_HIGH_RESOLUTION);
    accelerometer.setDataRate(MMA_800hz);
    accelerometer.setRange(MMA_RANGE_2G);

    accelerometer.setAutoSleep(true, 0x11, MMA_SLEEP_1_56hz);
    accelerometer.setWakeOnInterrupt(true);

    accelerometer.setMotionDetectionMode(MMA_MOTION, MMA_ALL_AXIS);
    accelerometer.setMotionTreshold(0x11);

    accelerometer.setInterruptsEnabled(MMA_AUTO_SLEEP | MMA_FREEFALL_MOTION);
    accelerometer.configureInterrupts(false, false);
    accelerometer.setInterruptPins(true, true, true, true, true, true);

    attachInterrupt(0, accelerometerInterruptHandler, FALLING);
  }
  else
  {
    Serial.println(F("failed. Check connections."));
    while (true) {};
  }
}

void loop()
{
  if (interrupt)
  {
    noInterrupts();
    interrupt = false;
    interrupts();

    bool wakeStateChanged;
    bool transient;
    bool landscapePortrait;
    bool tap;
    bool freefallMotion;
    bool dataReady;
    accelerometer.getInterruptEvent(&wakeStateChanged, &transient, &landscapePortrait, &tap, &freefallMotion, &dataReady);

    if (wakeStateChanged) {
      Serial.println(F("Wake state changed. Now: "));
      Serial.flush();
      mma8452_mode_t mode = accelerometer.getMode();
      Serial.println(mode);

      set_sleep_mode(SLEEP_MODE_PWR_DOWN);   // sleep mode is set here
      sleep_enable();
      sleep_mode();
      sleeping = true;
      // nap time.. ZZzz..

      // wait for interrupt to fire //

      // woke up from sleep
      sleeping = false;
      Serial.println(F("Woke up"));
      sleep_disable();
    }

    if (transient)
    {
      Serial.println(F("Transient"));
    }

    if (freefallMotion)
    {
      Serial.println(F("Motion"));
    }
    cli();
    digitalWrite(6, LOW);
    sei();
  }
  digitalWrite(5, HIGH);
  delay(100);
  digitalWrite(5, LOW);
  delay(100);
}

void accelerometerInterruptHandler()
{
  if (sleeping)
  {
    sleep_disable();
  }
  digitalWrite(6, HIGH);
  interrupt = true;
}
