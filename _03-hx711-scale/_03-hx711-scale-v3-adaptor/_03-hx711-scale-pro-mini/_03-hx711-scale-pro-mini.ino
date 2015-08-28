#include "HX711.h"
#include <Wire.h>
#include <Average.h>

HX711 scale(A0, A1);

#define DEBUG_OUT 0

const int nemoisonPin = 9;
volatile int isSent;
int measured = 0;
volatile int tosend   = 0;
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

  Wire.begin(2);
  Wire.onRequest(requestEvent);

  delay(5000);
  isSent = LOW;

  scale.set_scale(23040.f);
  scale.tare();
}


void loop()
{
  if ( DEBUG_OUT ) {
    Serial.print((millis() - startMills) * 0.001, 2);
    Serial.print("\t: ");
  }

  if ( isSent == LOW ) {
    float fmeasured = scale.get_units(10) ;
    measured = int(fmeasured * 1000) ;
    if ( DEBUG_OUT ) {
      Serial.print(fmeasured, 5);
      Serial.print("\t: ");
      Serial.print(measured);
    }
  }

  if (( isSent == LOW ) && ( measured < 7000 )) {
    tosend = measured;
    if ( DEBUG_OUT ) {
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

  scale.power_down();
  delay(50);
  scale.power_up();
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
