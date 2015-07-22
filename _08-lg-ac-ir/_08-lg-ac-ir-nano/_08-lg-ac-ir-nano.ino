#include <IRremote.h>

IRsend irsend;

const int AC_TYPE  = 0;
// 0 : TOWER, 1: WALL
int AC_POWER_ON    = 0;
// 0 : off , 1 : on
int AC_TEMPERATURE = 27;
// temperature : 18 ~ 30
int AC_FLOW        = 1;
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
	int AC_MSBITS0 = 8;
	int AC_MSBITS1 = 8;
	int AC_MSBITS2 = 0;

	int AC_MSBITS3;
	
	int AC_MSBITS4;
	int AC_MSBITS5;

	int AC_MSBITS6;

	if ( AC_POWER_ON == 0 ) {
		AC_MSBITS3  = 0;
		AC_POWER_ON = 1;
	} else {
		AC_MSBITS3  = 8;		
	}

	if ( temperature ) {
		AC_MSBITS4 = temperature - 15;
		AC_TEMPERATURE = temperature;
	} else {
		AC_MSBITS4 = AC_TEMPERATURE - 15;		
	}

	if ( air_flow ) {
		AC_MSBITS5 = air_flow;
		AC_FLOW = air_flow;
	} else {
		AC_MSBITS5 = AC_FLOW;
	}

    AC_MSBITS6 = AC_MSBITS2 ^ AC_MSBITS3 ^ AC_MSBITS4 ^ AC_MSBITS5;
    Serial.println(AC_MSBITS6);

}






void setup()
{
  Serial.begin(38400);
  ac_activate();
}

void loop() {

//      irsend.sendLGAC(0xa90, 28); 

}


