#include "HX711.h"
#include <Wire.h>
#include <Average.h>

HX711 scale(A0, A1);
Average<float> ave(10);

int nemoisonPin    = 9;
int nofchecked     = 0;
int typeofMeasured = 0;
int Measured       ;
int toI2cMeasured  ;

int MeasuredIsSent = LOW;

void setup()
{
  Serial.begin(38400);
  Serial.println("pet pad scale started");
  delay(100);

  pinMode(nemoisonPin, OUTPUT);
  digitalWrite(nemoisonPin, LOW);

  delay(500);

  scale.set_scale(23040.f);     // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();                 // reset the scale to 0

  Wire.begin(2);                // join i2c bus with address #2
  Wire.onRequest(requestEvent); // register event
}


void loop()
{
  Measured = int( scale.get_units(5) * 1000 );

  if ( Measured > 500 )
  {
    ave.push(Measured);
    if ( ((ave.maximum() - ave.minimum()) < 100 ) && ( ave.stddev() < 50) && ( nofchecked > 10 ) && ( ave.mean() > 1000 ) && ( ave.mean() < 10000 ) && ( MeasuredIsSent == LOW ) )
    {
      toI2cMeasured = int(ave.mean());
      typeofMeasured = 1;
    } else {
      toI2cMeasured = Measured;
    }
    digitalWrite(nemoisonPin, HIGH);
  } else {
    Measured       = 0;
    toI2cMeasured  = 0;
    nofchecked     = 0;
    typeofMeasured = 0;
    MeasuredIsSent = LOW;
    digitalWrite(nemoisonPin, LOW);
  }

  Serial.print("Measured: \t");
  Serial.print(Measured);
  Serial.print("\t");
  Serial.println(toI2cMeasured);

  nofchecked++;
  delay(500);

}

void requestEvent()
{
  byte myArray[3];
  myArray[0] = (toI2cMeasured >> 8 ) & 0xFF;
  myArray[1] = toI2cMeasured & 0xFF;
  myArray[2] = typeofMeasured;
  Wire.write(myArray, 3);
  if ( typeofMeasured == 1)  {
    MeasuredIsSent = HIGH;
    typeofMeasured = 0;
  }
}


