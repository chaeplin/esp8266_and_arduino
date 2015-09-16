#include "HX711.h"
#include <Wire.h>

// HX711.DOUT  - pin #A1
// HX711.PD_SCK - pin #A0

HX711 scale(A1, A0);  

int nemoisonPin = 10;
int Measured ;
int toI2cMeasured ;

void setup() {
  Serial.begin(38400);
  Serial.println("HX711 Demo");
  pinMode(nemoisonPin,OUTPUT);
  digitalWrite(nemoisonPin,LOW);

  delay(500);

  scale.set_scale(23040.f);     // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.tare();                 // reset the scale to 0
  
  Wire.begin(2);                // join i2c bus with address #2
  Wire.onRequest(requestEvent); // register event  
}


void loop() {
  Measured = int( scale.get_units(5) * 1000 );
  
  if ( Measured > 50 ) {
    toI2cMeasured = Measured ;
    digitalWrite(nemoisonPin,HIGH);
  } else {
    digitalWrite(nemoisonPin,LOW);
  }
  Serial.print("Measured: \t");
  Serial.print(Measured);
  Serial.print("\t");
  Serial.println(toI2cMeasured);
  scale.power_down();             // put the ADC in sleep mode
  delay(500);
  scale.power_up();
  
}

void requestEvent()
{
  byte myArray[2];
  myArray[0] = (toI2cMeasured >> 8 ) & 0xFF;
  myArray[1] = toI2cMeasured & 0xFF;
  
  Wire.write(myArray, 2); // respond with message of 6 bytes
}


