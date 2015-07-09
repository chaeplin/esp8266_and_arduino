/*
//
A0, A1 --> HX711
A3, A4 --> I2C to esp8266 4, 5

//
2 - int 0 - tilt sw * 2- gnd : INPUT_PULLUP
4 - LED

//
11 - esp reset : OUT - reset esp8266
//
10 - esp 13    : OUT - nemo is on pad
12 - esp 12    : OUT - vcc if ready

 */

#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "HX711.h"
#include <Wire.h>
#include <Vcc.h>

// tilt switch
const int wakeUpPin     = 2;

const int ledPowerPin  = 4;

//
const int espResetPin       = 11;
const int espnemoIsOnPadPin = 10;

volatile int Measured;
volatile long startMills;

// vcc
volatile int MeasuredIsSent = LOW;
float VccValue = 0;
int Attempt = 0;
int IsEspReseted = LOW;
int mqttMsgSent = LOW;

// vcc
const float VccMin   = 0.0;           // Minimum expected Vcc level, in Volts.
const float VccMax   = 3.4;           // Maximum expected Vcc level, in Volts.
const float VccCorrection = 1.0 / 1.0; // Measured Vcc by multimeter divided by reported Vcc

Vcc vcc(VccCorrection);

// HX711.DOUT  - pin #A1
// HX711.PD_SCK - pin #A0
HX711 scale(A1, A0);


// smoothing
// https://www.arduino.cc/en/Tutorial/Smoothing
// Define the number of samples to keep track of.  The higher the number,
// the more the readings will be smoothed, but the slower the output will
// respond to the input.  Using a constant rather than a normal variable lets
// use this value to determine the size of the readings array.

const int numReadings = 5;

int readings[numReadings];      // the readings from the analog input
int indexof = 0;                // the indexof of the current reading
int total = 0;                  // the running total
int average = 0;                // the average


void setup() {
  Serial.begin(38400);
  Serial.println("pet pad scale started");
  delay(100);

  Wire.begin(2);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  pinMode(wakeUpPin, INPUT_PULLUP);

  pinMode(ledPowerPin, OUTPUT);
  pinMode(espnemoIsOnPadPin, OUTPUT);
  pinMode(espResetPin, OUTPUT);

  digitalWrite(espnemoIsOnPadPin, LOW);
  digitalWrite(espResetPin, HIGH);
  digitalWrite(ledPowerPin, HIGH);

  startMills = millis();

  Serial.println("Initializing scale : start");

  scale.set_scale(23040.f);
  scale.tare();

  scale.power_down();
  Serial.println("Initializing scale : done");

  attachInterrupt(0, WakeUp, CHANGE);

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0 ;
  }

  sleepNow();

}

void sleepNow()
{
  Serial.println("Going sleep");

  scale.power_down();

  //
  MeasuredIsSent = LOW;
  Attempt = 0;

  //
  digitalWrite(espnemoIsOnPadPin, LOW);
  digitalWrite(ledPowerPin, LOW);

  //
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
  Serial.print(millis() - startMills);

  Measured = int( scale.get_units(3) * 1000 );

  Serial.print(" ====> weight : ");
  Serial.print(Measured);
  Serial.print(" ==> ");
  Serial.println(millis() - startMills);

  if ( Measured > 500 )
  {
    Serial.println("======> tilting detected, nemo is on pad");
    /*
    digitalWrite(espnemoIsOnPadPin, HIGH);
    espReset();
    */

  } else {
    Serial.println("======> tilting detected, but nemo is not on pad");
    sleepNow();
  }

}

void loop()
{

  //Serial.print("=====> checking weight :  ");
  //Serial.println(millis() - startMills);

  Measured = int( scale.get_units(5) * 1000 );

  Serial.print("==> weight : ");
  Serial.print(Measured);
  Serial.print(" ==> ");

  if (( Measured > 500 ) && ( Measured > (average - 200)) )
  {
    // Serial.println("======> nemo is on pad now");

    total = total - readings[indexof];
    readings[indexof] = Measured;
    total = total + readings[indexof];
    indexof = indexof + 1;

    if (indexof >= numReadings)
      indexof = 0;

    average = total / numReadings;

    Serial.print("  average ==>  ");
    Serial.println(average);

  } else {
    if ( Attempt == 1) {
      Serial.println("");
      Serial.print("============> nemo is not on pad now");
    }
    Serial.println("");
    Attempt++;
  }

  if ((( average > 1000 ) && ( Measured < 500) ) && ( IsEspReseted == LOW ))
  {
    VccValue = vcc.Read_Volts() * 1000 ;
    Serial.println("");
    Serial.print("==========> VCC ");
    Serial.println(VccValue);
    digitalWrite(espnemoIsOnPadPin, HIGH);
    espReset();
    IsEspReseted = HIGH;
    Attempt = 0;
    //delay(5000);
  }

  if (( Attempt == 10 ) && ( IsEspReseted == LOW ) )
  {
    Serial.println("======> Measurement has problem");
    sleepNow();
  }

  if (( Attempt == 20 ) && ( MeasuredIsSent == LOW ) && ( IsEspReseted == HIGH ))
  {
    Serial.println("======> I2C has problem");
    sleepNow();
  }

  if (( MeasuredIsSent == HIGH ) && ( Attempt == 1))
  {
    Serial.println("======> I2C msg has sent");
  }

  if (( MeasuredIsSent == HIGH) && ( mqttMsgSent == HIGH ))
  {
    Serial.println("======> mqtt msg has sent");
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
  byte myArray[4];

  myArray[0] = (average >> 8 ) & 0xFF;
  myArray[1] = average & 0xFF;
  myArray[2] = (int(VccValue) >> 8 ) & 0xFF;
  myArray[3] = int(VccValue) & 0xFF;

  Wire.write(myArray, 4);

  MeasuredIsSent = HIGH;
}

void receiveEvent(int howMany)
{
  int x = Wire.read();    // receive byte as an integer
  Serial.println(x);         // print the integer
  if ( x == 1 ) {
    mqttMsgSent = HIGH;
  }
}



