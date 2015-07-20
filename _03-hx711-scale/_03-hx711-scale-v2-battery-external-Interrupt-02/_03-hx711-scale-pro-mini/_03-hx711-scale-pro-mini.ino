
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include "HX711.h"
#include <Wire.h>
#include <Vcc.h>
#include <Average.h>

// tilt switch
const int wakeUpPin     = 2;
const int espResetPin   = 9;
const int ledPowerPin   = 10;


volatile int Measured;
volatile long startMills;

// vcc
volatile int MeasuredIsSent = LOW;
float VccValue = 0;
int Attempt = 0;
int IsEspReseted = LOW;
int mqttMsgSent = LOW;
int nofchecked = 0;
int averagetosend = 0;

// vcc
const float VccMin   = 0.0;           // Minimum expected Vcc level, in Volts.
const float VccMax   = 3.4;           // Maximum expected Vcc level, in Volts.
const float VccCorrection = 1.0 / 1.0; // Measured Vcc by multimeter divided by reported Vcc

Vcc vcc(VccCorrection);

// HX711.DOUT  - pin #A1
// HX711.PD_SCK - pin #A0
HX711 scale(A0, A1);

Average<float> ave(10);

void setup() {
  Serial.begin(38400);
  Serial.println("pet pad scale started");
  delay(100);

  Wire.begin(2);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  pinMode(wakeUpPin, INPUT_PULLUP);

  pinMode(ledPowerPin, OUTPUT);
  //pinMode(espnemoIsOnPadPin, OUTPUT);
  pinMode(espResetPin, OUTPUT);

  //digitalWrite(espnemoIsOnPadPin, LOW);
  digitalWrite(espResetPin, HIGH);
  digitalWrite(ledPowerPin, HIGH);

  startMills = millis();
  delay(200);
  Serial.println("Initializing scale : start");

  scale.set_scale(23040.f);
  scale.tare();

  scale.power_down();
  Serial.println("Initializing scale : done");

  attachInterrupt(0, WakeUp, CHANGE);

  sleepNow();

}

void sleepNow()
{
  Serial.println("Going sleep");

  scale.power_down();

  //digitalWrite(espnemoIsOnPadPin, LOW);
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

  MeasuredIsSent = LOW;
  mqttMsgSent = LOW;
  IsEspReseted = LOW;
  
  Attempt = 0;
  nofchecked = 0;
  ave.push(0);
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

  Measured = int( scale.get_units(5) * 1000 );

  Serial.print(" ====> weight : ");
  Serial.print(Measured);
  Serial.print(" ==> ");
  Serial.println(millis() - startMills);

  if ( Measured > 100 )
  {
    Serial.println("======> tilting detected, nemo is on pad");

  } else {
    Serial.println("======> tilting detected, but nemo is not on pad");
    sleepNow();
  }

}

void loop()
{

  if ( IsEspReseted == LOW )
  {
    Measured = int( scale.get_units(5) * 1000 );
    Serial.print("==> weight : ");
    Serial.print(Measured);
    Serial.print(" ==> no ==> ");
    Serial.print(nofchecked);
    Serial.print(" ==> avg ==> ");
    Serial.print(ave.mean());  
    Serial.print(" ==> max - min ==> ");
    Serial.print( ave.maximum() - ave.minimum() );  
    Serial.print(" ==> stddev ==> ");
    Serial.println(ave.stddev());            

    if ( Measured > 500 ) {
      ave.push(Measured);
      Attempt = 0 ;
    }

    if ( ((ave.maximum() - ave.minimum()) < 100 ) && ( ave.stddev() < 50) && ( nofchecked > 10 ) && (int(ave.mean()) < 10000) )
    {
      averagetosend = int(ave.mean());
      VccValue = vcc.Read_Volts() * 1000 ;

      espReset();
      scale.power_down();
      IsEspReseted = HIGH;

      Attempt = 0;

      Serial.println("");
      Serial.print("==========> averagetosend ");
      Serial.print(ave.mean()) ;
      Serial.print("   =====> VCC ");
      Serial.println(VccValue);

    }

    if ( Measured < 500 )  {
      if ( Attempt == 0 )
      {
        Serial.println("");
        Serial.print("============> nemo is not on pad now");
      }
      Attempt++;
    }

    if ( Attempt == 20 ) {
      Serial.println("======> Measurement has problem");
      sleepNow();
    }

    nofchecked++;
    delay(100);

  }

  
  if (  IsEspReseted == HIGH )  
  {
    Attempt++;
    if ( Attempt == 20 )
    {
      Serial.println("======> I2C has problem");
      sleepNow();
    }

    if ( mqttMsgSent == HIGH )
    {
      Serial.println("======> mqtt msg has sent");
      digitalWrite(ledPowerPin, LOW);
      delay(300);
      digitalWrite(ledPowerPin, HIGH);
      IsEspReseted = LOW;
      sleepNow();
    }

    delay(1000);
  }

}

void espReset()
{
  Serial.println("Reset ESP");
  digitalWrite(espResetPin, LOW);
  delay(10);
  digitalWrite(espResetPin, HIGH);

  digitalWrite(ledPowerPin, LOW);
  delay(100);
  digitalWrite(ledPowerPin, HIGH);
  delay(100);
  digitalWrite(ledPowerPin, LOW);
  delay(100);
  digitalWrite(ledPowerPin, HIGH);

}

void requestEvent()
{
  byte myArray[4];

  myArray[0] = (averagetosend >> 8 ) & 0xFF;
  myArray[1] = averagetosend & 0xFF;
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



