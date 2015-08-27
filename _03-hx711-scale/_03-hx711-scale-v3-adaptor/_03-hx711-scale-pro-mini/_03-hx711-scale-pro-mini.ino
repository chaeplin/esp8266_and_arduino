#include "HX711.h"
#include <Wire.h>
#include <Average.h>

HX711 scale(A0, A1);

#define DEBUG_OUT 0

const int nemoisonPin = 9;
volatile int measured = 0;
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

  delay(5000);

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
    
  float fmeasured = scale.get_units(10) ; 
  measured = int(fmeasured * 1000) ;

  if ( DEBUG_OUT ) {
    Serial.print(fmeasured, 5);
    Serial.print("\t: ");
    Serial.print(measured);    
  }

  if (( measured < 0 ) || ( measured > 7000 )) {
    measured = 0;
  }
  
  if ( DEBUG_OUT ) {
    Serial.print("\t:\t");
    Serial.println(measured);
  }
  
  tosend = measured;

  if ( measured > 500 )
  {
    digitalWrite(nemoisonPin, HIGH);
  } else {
    digitalWrite(nemoisonPin, LOW);
  }

  //scale.power_down();
  delay(200);
  //scale.power_up();
}

void requestEvent()
{
  byte myArray[2];
  myArray[0] = (tosend >> 8 ) & 0xFF;
  myArray[1] = tosend & 0xFF;
  Wire.write(myArray, 2);
}
