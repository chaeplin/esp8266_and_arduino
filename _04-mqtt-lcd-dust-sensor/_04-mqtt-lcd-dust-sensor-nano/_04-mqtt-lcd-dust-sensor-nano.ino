#include <IRremote.h>
#include <Wire.h>

IRsend irsend;

// IR 
// D3 is out
const int AC_TYPE  = 0;
// 0 : TOWER
// 1 : WALL

int AC_POWER_ON    = 0;
// 0 : off
// 1 : on

int AC_TEMPERATURE = 27;
// temperature : 18 ~ 30

int AC_FLOW        = 1;
// 0 : low
// 1 : mid
// 2 : high
// 3 : rotate

int AC_AIR_ACLEAN  = 0;
// 0 : off
// 1 : on

unsigned long AC_CODE_TO_SEND;

int r = LOW;
int o_r = LOW;

byte a, b;

// IR

/*
 Standalone Sketch to use with a Arduino UNO and a
 Sharp Optical Dust Sensor GP2Y1010AU0F
*/
  
int measurePin = A6; //Connect dust sensor to Arduino A0 pin
int ledPower = 2;   //Connect 3 led driver pins of dust sensor to Arduino D2
  
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

int voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

// IR
void ac_change_air_swing(int air_swing)
{
  if ( AC_TYPE == 0) {
    if ( air_swing == 1) {
      AC_CODE_TO_SEND = 0x881316B;
    } else {
      AC_CODE_TO_SEND = 0x881317C;
    }
  } else {
    if ( air_swing == 1) {
      AC_CODE_TO_SEND = 0x8813149;
    } else {
      AC_CODE_TO_SEND = 0x881315A;
    }
  }

  Serial.print("code to send : ");
  Serial.println(AC_CODE_TO_SEND, BIN);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);

}

void ac_power_down()
{
  AC_CODE_TO_SEND = 0x88C0051;

  Serial.print("code to send : ");
  Serial.println(AC_CODE_TO_SEND, BIN);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);

  AC_POWER_ON = 0;
}

void ac_air_clean(int air_clean)
{
  if ( air_clean == 1) {
    AC_CODE_TO_SEND = 0x88C000C;
  } else {
    AC_CODE_TO_SEND = 0x88C0084;
  }
  Serial.print("code to send : ");
  Serial.println(AC_CODE_TO_SEND, BIN);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);

  AC_AIR_ACLEAN = air_clean;
}

void ac_activate(int temperature, int air_flow)
{

  int AC_MSBITS1 = 8;
  int AC_MSBITS2 = 8;
  int AC_MSBITS3 = 0;
  int AC_MSBITS4 = 0;
  int AC_MSBITS5 = temperature - 15;
  int AC_MSBITS6 ;



  if ( AC_TYPE == 0) {
    if ( air_flow == 0 ) {
      AC_MSBITS6 = 0;
    }  
    if ( air_flow == 1 ) {
      AC_MSBITS6 = 4;
    }
    if ( air_flow == 2 ) {
      AC_MSBITS6 = 6;
    }
    if ( air_flow == 3 ) {
      AC_MSBITS6 = 12;
    }
  } else {
    if ( air_flow == 0 ) {
      AC_MSBITS6 = 0;
    }  
    if ( air_flow == 1 ) {
      AC_MSBITS6 = 2;
    }
    if ( air_flow == 2 ) {
      AC_MSBITS6 = 4;
    }
    if ( air_flow == 3 ) {
      AC_MSBITS6 = 5;
    }

  }

  int AC_MSBITS7 = (AC_MSBITS3 + AC_MSBITS4 + AC_MSBITS5 + AC_MSBITS6) & B00001111;

  AC_CODE_TO_SEND =  AC_MSBITS1 << 4 ;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS2) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS3) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS4) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS5) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS6) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS7);

  Serial.print("code to send : ");
  Serial.println(AC_CODE_TO_SEND, BIN);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);

  AC_POWER_ON = 1;
  AC_TEMPERATURE = temperature;
  AC_FLOW = air_flow;
}

// IR

void setup()
{
  Serial.begin(38400);
  Serial.println("Starting dust Sensor");
  pinMode(ledPower,OUTPUT);
  Wire.begin(2);                // join i2c bus with address #2
  Wire.onRequest(requestEvent); // register event
  Wire.onReceive(receiveEvent);

}

void loop()
{
  digitalWrite(ledPower, LOW); // power on the LED
  delayMicroseconds(samplingTime);
  
  voMeasured = analogRead(measurePin); // read the dust value
  
  delayMicroseconds(deltaTime);
  digitalWrite(ledPower, HIGH); // turn the LED off
  delayMicroseconds(sleepTime);

  if ( r != o_r) {

    // a : mode or temp
    // 18 ~ 30 : temp
    // 0 : off
    // 1 : on
    // 2 : air_swing
    // 3 : air_clean
    // 4 : air_flow
    // 5 : temp
    //
    // b : air_flow, temp, swing, clean

    Serial.print("a : ");
    Serial.print(a);
    Serial.print("  b : ");
    Serial.println(b);   

    switch (a) {
      case 0:
        ac_power_down();
        break;
      case 1:
        ac_activate(AC_TEMPERATURE, AC_FLOW);
        break;
      case 2:
        if ( b == 0 | b == 1 ) {
          ac_change_air_swing(b);
        }      
        break;
      case 3:
        if ( b == 0 | b == 1 ) {
          ac_air_clean(b);
        }      
        break;
      case 4:
        if ( 0 <= b && b <= 3  ) {
          ac_activate(AC_TEMPERATURE, b);
        }      
        break;
      case 5:
        if (18 <= b && b <= 30  ) {
          ac_activate(b, AC_FLOW);
        }      
        break;
      default:
        if ( 18 <= a && a <= 30 ) {
          if ( 0 <= b && b <= 3 ) {
            ac_activate(a, b);
          }
        }
    }

    o_r = r ;
  }
  delay(100);

}

// function that executes whenever data is requested by master
// this function is registered as an event, see setup()
void requestEvent()
{ 
  byte myArray[2];
  myArray[0] = (voMeasured >> 8 ) & 0xFF;
  myArray[1] = voMeasured & 0xFF;

  Wire.write(myArray, 2);
}

void receiveEvent(int howMany)
{
  a = Wire.read();
  b = Wire.read();
  r = !r ;
}

