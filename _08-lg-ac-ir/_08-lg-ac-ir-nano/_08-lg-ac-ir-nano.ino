#include <IRremote.h>

IRsend irsend;

const int AC_TYPE  = 0;
// 0 : TOWER, 1: WALL
int AC_POWER_ON    = 0;
// 0 : off , 1 : on
int AC_TEMPERATURE = 27;
// temperature : 18 ~ 30
int AC_FLOW        = 2;
// 0 : low, 2 : mid , 4 : high, 5 : rotate

unsigned long AC_CODE_TO_SEND;

void ac_change_air_swing(int air_swing)
{
  // air_move    : 0 - fixed, 1 - swing
  //!상하바람				1000 1000 0001 0011 0001 0100 1001
  //!상하 off				1000 1000 0001 0011 0001 0101 1010
  //
  //!좌우바람				1000 1000 0001 0011 0001 0110 1011
  //!좌우off				1000 1000 0001 0011 0001 0111 1100
}

void ac_power_down()
{
  // !off				1000 1000 1100 0000 0000 0101 0001
}

void ac_air_clean()
{
  // !공기청정				1000 1000 1100 0000 0000 0000 1100 // on
  // !					1000 1000 1100 0000 0000 1000 0100 // off
}

void ac_activate(int temperature, int air_flow)
{
  int AC_MSBITS1 = 8;
  int AC_MSBITS2 = 8;
  int AC_MSBITS3 = 0;

  int AC_MSBITS4;

  int AC_MSBITS5;
  int AC_MSBITS6;

  int AC_MSBITS7;

  if ( AC_POWER_ON == 0 ) {
    AC_MSBITS4  = 0;
    AC_POWER_ON = 1;
  } else {
    AC_MSBITS4  = 8;
  }

  if ( temperature ) {
    AC_MSBITS5 = temperature - 15;
    AC_TEMPERATURE = temperature;
  } else {
    AC_MSBITS5 = AC_TEMPERATURE - 15;
  }

  if ( air_flow ) {
    AC_MSBITS6 = air_flow;
    AC_FLOW = air_flow;
  } else {
    AC_MSBITS6 = AC_FLOW;
  }

  AC_MSBITS7 =  (AC_MSBITS3 + AC_MSBITS4 + AC_MSBITS5 + AC_MSBITS6) & B00001111;

  AC_CODE_TO_SEND =  AC_MSBITS1 << 4 ;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS2) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS3) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS4) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS5) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS6) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS7);

  irsend.sendLGAC(AC_CODE_TO_SEND, 28);
  Serial.print(" mode ");
  Serial.print(AC_POWER_ON);
  Serial.print(" temp ");
  Serial.print(temperature);
  Serial.print(" air ");
  Serial.print(air_flow);
  Serial.print(" bin ");
  Serial.println(AC_CODE_TO_SEND, BIN);
  
}






void setup()
{
  Serial.begin(38400);
  delay(1000);
  Serial.println(" ");

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
  delay(5000);
}

void loop() {

  //      irsend.sendLGAC(0xa90, 28);

}


