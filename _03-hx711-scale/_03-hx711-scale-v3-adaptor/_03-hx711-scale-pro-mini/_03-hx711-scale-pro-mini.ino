#include "HX711.h"
#include <Wire.h>
#include <Average.h>

HX711 scale(A0, A1);

#define DEBUG_OUT 1

const int nemoisonPin = 9;
int isSent;
int measured = 0;
int tosend   = 0;
long startMills;

void setup()
{
  Serial.begin(38400);
  if ( DEBUG_OUT ) {
    Serial.println("pet pad scale started");
    delay(100);
  }

  startMills = millis();

  pinMode(nemoisonPin, OUTPUT);
  digitalWrite(nemoisonPin, LOW);

  delay(5000);
  isSent = LOW;

  scale.set_scale(23040.f);
  scale.tare();

  Wire.begin(2);
  Wire.onRequest(requestEvent);
}


void loop()
{
  if ( DEBUG_OUT ) {
    Serial.print((millis() - startMills) * 0.001, 2);
    Serial.print("\t: ");
  }

  if ( isSent == LOW ) {
    //scale.power_up();
    float fmeasured = scale.get_units(10) ;
    measured = int(fmeasured * 1000) ;
    tosend = measured;

    if ( DEBUG_OUT ) {
      Serial.print(fmeasured, 5);
      Serial.print("\t: ");
      Serial.print(measured);
      Serial.print("\t:\t");
      Serial.println(tosend);
    }

  }

  if ( measured > 500 )
  {
    digitalWrite(nemoisonPin, HIGH);
  } else {
    digitalWrite(nemoisonPin, LOW);
  }

  //scale.power_down();
  delay(200);
}

void requestEvent()
{
  isSent = HIGH;
  byte myArray[3];
  myArray[0] = (abs(tosend) >> 8 ) & 0xFF;
  myArray[1] = abs(tosend) & 0xFF;
  if ( tosend < 0 ) {
    myArray[2] = 1;
  } else {
    myArray[2] = 0;
  }

  Wire.write(myArray, 3);
  isSent = LOW;
}
