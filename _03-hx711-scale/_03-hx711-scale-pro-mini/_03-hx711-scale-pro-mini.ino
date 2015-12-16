#include "HX711.h"
#include <Wire.h>
#include <Average.h>
#include "Timer.h"
#include <Time.h>

HX711 scale(A0, A1);

#define DEBUG_OUT 0

const int nemoisonPin = 9;
volatile int measured = 0;
volatile int tosend   = 0;
volatile int isSent   = LOW;
volatile int o_isSent = LOW;
unsigned long startMills;

// Timer
Timer t;

void setup()
{
  Serial.begin(115200);
  if ( DEBUG_OUT ) {
    Serial.println("pet pad scale started");
    delay(100);
  }

  startMills = millis();

  pinMode(nemoisonPin, OUTPUT);
  digitalWrite(nemoisonPin, LOW);

  delay(1000);

  scale.set_scale(231.f);
  //scale.set_scale(23040.f);
  scale.tare();

  Wire.begin(2);
  Wire.onRequest(requestEvent);

  // event Timer
  int updateEvent = t.every(500, doUpdateHX711);

}

void doUpdateHX711()
{
  volatile float fmeasured = scale.get_units(5) ;
  measured = int(fmeasured * 10) ;
  tosend = measured;
  isSent = ! isSent;
}

void loop()
{

  t.update();

  if ( isSent != o_isSent )
  {
    notifyesp8266();
    o_isSent = isSent;
    if ( DEBUG_OUT )
    {
      Serial.print((millis() - startMills) * 0.001, 2);
      Serial.print("\t: ");
      Serial.print(measured);
      Serial.print("\t:\t");
      Serial.println(tosend);
    }
  }
}

void notifyesp8266()
{
  digitalWrite(nemoisonPin, HIGH);
  delay(30);
  digitalWrite(nemoisonPin, LOW);
}

void requestEvent()
{
  byte myArray[3];
  myArray[0] = (abs(tosend) >> 8 ) & 0xFF;
  myArray[1] = abs(tosend) & 0xFF;
  if ( tosend < 0 ) {
    myArray[2] = 1;
  } else {
    myArray[2] = 0;
  }

  Wire.write(myArray, 3);
}
