/*

A0, A1 --> HX711
A3, A4 --> I2C to esp8266 4, 5

2 - int 0 - tilt sw - gnd : INPUT_PULLUP
3 - int 1 - tilt sw - gnd : INPUT_PULLUP
4 - hx711 power : OUT

10 - esp 13    : OUT   - nemo is on pad
11 - esp reset : OUT   - reset esp8266
12 - esp 12    : INPUT - wifi/mqtt status

 */

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "HX711.h"
#include <Wire.h>

// tilt switch
int wakeUp1Pin     = 2;
int wakeUp2Pin     = 3;

//
int hx711PowerPin  = 4;

// int espnemoIsonPadPin = 10;
int espResetpin       = 11;
int espRfStatePin     = 12; 

int Measured ;
int toI2cMeasured ;

long startMills;

int nemoIsoffPad ;

int espRfstate = HIGH;

// HX711.DOUT  - pin #A1
// HX711.PD_SCK - pin #A0
HX711 scale(A1, A0);  

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


void setup() {
  Serial.begin(38400);
  Serial.println("pet pad scale started");
  delay(100);

  pinMode(wakeUp1Pin, INPUT_PULLUP);
  pinMode(wakeUp2Pin, INPUT_PULLUP);

  pinMode(hx711PowerPin, OUTPUT);
  
 // pinMode(espnemoIsonPadPin, OUTPUT);
  pinMode(espResetpin, OUTPUT);
  pinMode(espRfStatePin, INPUT);

  digitalWrite(hx711PowerPin, HIGH)
 // digitalWrite(espnemoIsonPadPin, LOW)
  digitalWrite(espResetPin, HIGH);

  attachInterrupt(0, WakeUp, CHANGE);
  attachInterrupt(1, WakeUp, CHANGE);

  startMills = millis();

  scale.set_scale(23040.f);  
  scale.tare();                 
  
  Wire.begin(2);                
  Wire.onRequest(requestEvent); 

  scale.power_down();
  delay(250);

  for (int thisReading = 0; thisReading < numReadings; thisReading++)
    readings[thisReading] = 0 ;

  nemoIsoffPad = 0;
  sleepNow();

}

void sleepNow()
{
  Serial.println("Going sleep");
  delay(100);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, WakeUp, CHANGE);
  attachInterrupt(1, WakeUp, CHANGE);

  sleep_mode();   
  sleep_disable();
  detachInterrupt(0);
  detachInterrupt(1); 

  scale.power_up();
  delay(400);
  Serial.println("Wake up at sleepNow");
  
}

void WakeUp()   
{ 
    Serial.print("======> Wake up :  ");
    Serial.println(millis() - startMills);
    espRfstate = HIGH;
}


void loop()
{

  Measured = int( scale.get_units(5) * 1000 );

  if ( Measured > 500 ) {

    total = total - readings[indexof];
    readings[indexof] = Measured;
    total = total + readings[indexof];
    indexof = indexof + 1;
    if (indexof >= numReadings)
      indexof = 0;

    average = total / numReadings;  

  }

  if ( average > 500 ) {
      toI2cMeasured = average;
  }

  if ( Measured < 500 ) {
      nemoIsoffPad = nemoIsoffPad + 1 ;
  }
  
  if ( nemoIsoffPad > 10 ) {
      espReset();
      while ( digitalRead(espRfStatePin) != LOW ) {
            delay(500);
            Serial.print(".");        
      }

      Serial.println(millis() - startMills);
      Serial.println("msg sent");
      scale.power_down();
      sleepNow();
  }

  delay(10);


}

/*
void loop() {
  
  Serial.println(millis() - startMills);

  Measured = int( scale.get_units(10) * 1000 );
  
  if ( Measured > 50 ) {
    toI2cMeasured = Measured ;
    digitalWrite(espRfStatePin,HIGH);
  } else {
    digitalWrite(espRfStatePin,LOW);
  }
  Serial.print("Measured: \t");
  Serial.print(Measured);
  Serial.print("\t");
  Serial.println(toI2cMeasured);
  
  scale.power_down();             // put the ADC in sleep mode
  delay(100);
  scale.power_up();
  
}
*/

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
  myArray[0] = (toI2cMeasured >> 8 ) & 0xFF;
  myArray[1] = toI2cMeasured & 0xFF;
  
  Wire.write(myArray, 2); // respond with message of 6 bytes
}


