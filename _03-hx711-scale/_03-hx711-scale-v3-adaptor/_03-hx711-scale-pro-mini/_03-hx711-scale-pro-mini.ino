#include "HX711.h"
#include <Wire.h>
#include <Average.h>

HX711 scale(A0, A1);

#define DEBUG_OUT 1

int nemoisonPin    = 9;
int measured       = 0;
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

  delay(1000);

  scale.set_scale(23040.f);
  scale.tare();

  Wire.begin(2);
  Wire.onRequest(requestEvent);
}


void loop()
{
  measured = int( scale.get_units(10) * 1000 );
  if ( measured < 0 ) { measured = 0; }

  if ( measured > 200 )
  {
    digitalWrite(nemoisonPin, HIGH);
  } else {
    digitalWrite(nemoisonPin, LOW);
  }

  if ( DEBUG_OUT ) {
    Serial.print((millis() - startMills) * 0.001, 2);
    Serial.print(" : ");
    Serial.println(measured);
  }
  delay(100);
}

void requestEvent()
{
  byte myArray[2];
  myArray[0] = (measured >> 8 ) & 0xFF;
  myArray[1] = measured & 0xFF;
  Wire.write(myArray, 2);
}
