#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <Average.h>

#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#include <Wire.h>

// wifi
#ifdef __IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

#define nemoisOnPin 14
#define ledPin 13

Average<float> ave(10);

volatile int measured = 0;
volatile int inuse = LOW;
volatile int r = LOW;
int o_r = LOW;

void setup() {
  Serial.begin(38400);
  Wire.begin(4, 5);
  Serial.println("HX711 START");
  delay(100);

  pinMode(nemoisOnPin, INPUT);
  pinMode(ledPin, OUTPUT);

  Serial.println();

  attachInterrupt(14, hx711IsReady, RISING);
}



void loop()
{ 
  if ( r != o_r ) 
  {
    Serial.print(inuse);
    Serial.print(" : ");
    Serial.println(measured);
    o_r = r;
  }
  
}


void hx711IsReady()
{

  Wire.requestFrom(2, 3);

  int x;
  byte a, b, c;

  a = Wire.read();
  b = Wire.read();
  c = Wire.read();


  x = a;
  x = x << 8 | b;

  if ( x >= 7000 ) {
    x = 0;
  }

  if ( c == 1 ) {
    x = x * -1;
  }

  if ( x > 500 ) {
    inuse = HIGH;
  } else {
    inuse = LOW;
  }
  measured = x;
  r = !r;
}
