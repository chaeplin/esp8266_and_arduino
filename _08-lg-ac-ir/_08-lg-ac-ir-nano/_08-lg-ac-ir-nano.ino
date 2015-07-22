#include <IRremote.h>
#include <Wire.h>

IRsend irsend;

const int AC_TYPE  = 0;
// 0 : TOWER
// 1 : WALL

int AC_POWER_ON    = 0;
// 0 : off 
// 1 : on

int AC_TEMPERATURE = 27;
// temperature : 18 ~ 30

int AC_FLOW        = 2;
// 0 : low
// 2 : mid
// 4 : high
// 5 : rotate

int AC_AIR_ACLEAN  = 0;
// 0 : off
// 1 : on


unsigned long AC_CODE_TO_SEND;

int r = LOW;
int o_r = LOW;

byte a, b;

void ac_change_air_swing(int air_swing)
{
  if ( AC_TYPE == 0) {
      if ( air_swing == 1) {
           AC_CODE_TO_SEND = B1000100000010011000101101011;
      } else {
           AC_CODE_TO_SEND = B1000100000010011000101111100;
      }
  } else {
      if ( air_swing == 1) {
           AC_CODE_TO_SEND = B1000100000010011000101001001;
      } else {
           AC_CODE_TO_SEND = B1000100000010011000101011010;
      }
  }
  
  Serial.print("code to send : ");
  Serial.println(AC_CODE_TO_SEND, BIN);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);

}

void ac_power_down()
{
  AC_CODE_TO_SEND = B1000100011000000000001010001;

  Serial.print("code to send : ");
  Serial.println(AC_CODE_TO_SEND, BIN);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);

  AC_POWER_ON = 0;
}

void ac_air_clean(int air_clean)
{
  if ( air_clean == 1) {
     AC_CODE_TO_SEND = B1000100011000000000000001100;
  } else {
    AC_CODE_TO_SEND = B1000100011000000000010000100;
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
  int AC_MSBITS6 = air_flow;
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


void setup()
{
  Serial.begin(38400);
  delay(1000);
  Wire.begin(7);
  Wire.onReceive(receiveEvent);

  Serial.println("  - - - T E S T - - -   ");

  ac_activate(AC_TEMPERATURE, AC_FLOW);
  delay(5000);
  ac_activate(25, 2);
  delay(5000);
  ac_activate(26, 2);
  delay(5000);
  ac_activate(27, 2);
  delay(5000);
  ac_activate(28, 2);
  delay(5000);
  ac_activate(28, 4);
  delay(5000);
  ac_activate(28, 5);
  delay(5000);
  ac_activate(28, 0);
  delay(5000);
  ac_activate(28, 4);

}

void loop() 
{
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
  
    if ( 18 <= a <= 30 ) {
      if ( b == 0 | b == 2 | b == 4 | b == 5) {
        ac_activate(a, b);
      }
    }

    if ( a == 0 ) {
      ac_power_down();
    }

    if ( a == 1 ) {
      ac_activate(AC_TEMPERATURE, AC_FLOW);
    }

    if ( a == 2 ) {
      if ( b == 0 | b == 1 ) {
        ac_change_air_swing(b); 
      }     
    }

    if ( a == 3 ) {
      if ( b == 0 | b == 1 ) {
        ac_air_clean(b);
    }

    if ( a == 4 ) {
      if ( b == 0 | b == 2 | b == 4 | b == 5) {
        ac_activate(AC_TEMPERATURE, b);
      }
    }

    if ( a == 5 ) {
      if ( 18 <= b <= 30 ){
        ac_activate(b, AC_FLOW);
      }
    }

    o_r = r ;
  }

}


void receiveEvent(int howMany)
{
  a = Wire.read();
  b = Wire.read();
  r = !r ;
}


