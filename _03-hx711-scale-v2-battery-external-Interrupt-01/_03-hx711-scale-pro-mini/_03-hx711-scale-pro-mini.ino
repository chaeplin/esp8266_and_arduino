/*

A0, A1 --> HX711
A3, A4 --> I2C to esp8266 4, 5

2 - int 0 - tilt sw * 2- gnd : INPUT_PULLUP
4 - LED

10 - esp 13    : OUT   - nemo is on pad
11 - esp reset : OUT   - reset esp8266
12 - esp 12    : INPUT - wifi/mqtt status

 */

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "HX711.h"
#include <Wire.h>

// tilt switch
int wakeUpPin     = 2;

//
int ledPowerPin  = 4;

int espnemoIsOnPadPin = 10;
int espResetPin       = 11;
//int espRfStatePin     = 12;

volatile int Measured ;
//int espRfstate ;

volatile long startMills;


// HX711.DOUT  - pin #A1
// HX711.PD_SCK - pin #A0
HX711 scale(A1, A0);

/*
// smoothing
// https://www.arduino.cc/en/Tutorial/Smoothing
// Define the number of samples to keep track of.  The higher the number,
// the more the readings will be smoothed, but the slower the output will
// respond to the input.  Using a constant rather than a normal variable lets
// use this value to determine the size of the readings array.

const int numReadings = 10;

int readings[numReadings];      // the readings from the analog input
int indexof = 0;                // the indexof of the current reading
int total = 0;                  // the running total
int average = 0;                // the average

*/

void setup() {
  Serial.begin(38400);
  Serial.println("pet pad scale started");
  delay(100);
  
  Wire.begin(2);
  Wire.onRequest(requestEvent);

  pinMode(wakeUpPin, INPUT_PULLUP);

  pinMode(ledPowerPin, OUTPUT);
  pinMode(espnemoIsOnPadPin, OUTPUT);
  pinMode(espResetPin, OUTPUT);
  //pinMode(espRfStatePin, INPUT);

  digitalWrite(espnemoIsOnPadPin, LOW);
  digitalWrite(espResetPin, LOW);
  digitalWrite(ledPowerPin, HIGH);

  startMills = millis();

  Serial.println("Initializing scale : start");
  //delay(2000);

  scale.set_scale(23040.f);
  scale.tare();

  scale.power_down();
  Serial.println("Initializing scale : done");

  attachInterrupt(0, WakeUp, CHANGE);


  /*
    for (int thisReading = 0; thisReading < numReadings; thisReading++)
      readings[thisReading] = 0 ;
  */

  sleepNow();
    

}

void sleepNow()
{
  Serial.println("Going sleep");

  scale.power_down();
  digitalWrite(espnemoIsOnPadPin, LOW);
  digitalWrite(ledPowerPin, LOW);

  delay(100);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, WakeUp, CHANGE);
  
  sleep_mode();
  
  sleep_disable();
  detachInterrupt(0);

  Serial.println("Wake up at sleepNow");
  Serial.println(millis() - startMills);

  check_pad_status();
}

void WakeUp()
{
  Serial.print("======> Wake up :  ");
  startMills = millis();
  Serial.println(millis() - startMills);
  scale.power_up();

}

void check_pad_status()
{  
  digitalWrite(ledPowerPin, HIGH);

  Serial.print("======> checking pad :  ");
  delay(500);
  Serial.println(millis() - startMills);

  Measured = int( scale.get_units(3) * 1000 );

  Serial.print("======> weight : ");
  Serial.print(Measured);
  Serial.print(" ==> ");
  Serial.println(millis() - startMills);

  if ( Measured > 500 )
  {
    Serial.println("======> tilting detected, nemo is on pad");
    digitalWrite(espnemoIsOnPadPin, HIGH);
    espReset();
  } else {
    Serial.println("======> tilting detected, but nemo is not on pad");
    sleepNow();
  }

}

void loop()
{
  Serial.print("======> checking weight :  ");
  Serial.println(millis() - startMills);  

  Measured = int( scale.get_units(5) * 1000 );

  Serial.print("======> weight : ");
  Serial.print(Measured);
  Serial.print(" ==> ");
  Serial.println(millis() - startMills);
    
  if ( Measured > 500 )
  {
    Serial.println("======> nemo is on pad now");
  } else {
    Serial.println("======> nemo is not on pad");
    sleepNow();
  }
  delay(500);
}

void espReset()
{
  Serial.println("Reset ESP");
  digitalWrite(espResetPin, LOW);
  delay(10);
  digitalWrite(espResetPin, HIGH);
  
}

void requestEvent()
{
  byte myArray[2];
  myArray[0] = (Measured >> 8 ) & 0xFF;
  myArray[1] = Measured & 0xFF;

  Wire.write(myArray, 2); // respond with message of 6 bytes
}


