#include "HX711.h"
#include <Wire.h>

HX711 scale(A0, A1);

#define DEBUG_OUT 1

const int nemoisonPin = 9;
volatile int measured = 0;
volatile int isSent   = LOW;
volatile int o_isSent = LOW;
unsigned long startMills;

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

  scale.set_scale(23100.f);
  //scale.set_scale(23040.f);
  scale.tare();

  Wire.begin(2);
  Wire.onRequest(requestEvent);

}

void loop()
{
  if ( ( millis() - startMills ) > 100 ) {
    volatile float fmeasured = scale.get_units(5) ;
    measured = int(fmeasured * 1000) ;

    notifyesp8266();

    if ( DEBUG_OUT )
    {
      Serial.print(millis() * 0.001, 2);
      Serial.print("\t: ");
      Serial.println(measured);
    }
    startMills = millis();
  }
}

void notifyesp8266()
{
  digitalWrite(nemoisonPin, HIGH);
  delay(10);
  digitalWrite(nemoisonPin, LOW);
}

void requestEvent()
{
  byte myArray[3];
  myArray[0] = (abs(measured) >> 8 ) & 0xFF;
  myArray[1] = abs(measured) & 0xFF;
  if ( measured < 0 ) {
    myArray[2] = 1;
  } else {
    myArray[2] = 0;
  }

  Wire.write(myArray, 3);

}

