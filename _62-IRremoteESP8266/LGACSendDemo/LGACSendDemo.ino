#include <IRremoteESP8266.h>

#define GND_FOR_IR 13
#define SIG_FOR_IR 12

const int AC_TYPE  = 0;  // 0 : TOWER, 1 : WALL
int AC_HEAT        = 0;  // 0 : cooling,  1 : heating
int AC_POWER_ON    = 0;  // 0 : off,  1 : on
int AC_AIR_ACLEAN  = 0;  // 0 : off, 1 : on --> power on
int AC_TEMPERATURE = 27; // temperature : 18 ~ 30
int AC_FLOW        = 1;  // 0 : low, 1 : mid, 2 : high,  if AC_TYPE =1, 3 : change
const int AC_FLOW_TOWER[3] = {0, 4, 6};
const int AC_FLOW_WALL[4]  = {0, 2, 4, 5};
unsigned long AC_CODE_TO_SEND;

IRsend irsend(SIG_FOR_IR); //an IR led is connected to GPIO pin SIG_FOR_IR

void ac_send_code(unsigned long code) {
  Serial.print("code to send : ");
  Serial.print(code, BIN);
  Serial.print(" : ");
  Serial.println(code, HEX);

  irsend.sendLG(code, 28);
}

void ac_activate(int temperature, int air_flow) {

  int AC_MSBITS1 = 8;
  int AC_MSBITS2 = 8;
  int AC_MSBITS3 = 0;
  int AC_MSBITS4 ;
  if ( AC_HEAT == 1 ) {
    // heating
    AC_MSBITS4 = 4;
  } else {
    // cooling
    AC_MSBITS4 = 0;
  }
  
  int AC_MSBITS5 = temperature - 15;
  int AC_MSBITS6 ;

  if ( AC_TYPE == 0) {
    AC_MSBITS6 = AC_FLOW_TOWER[air_flow];
  } else {
    AC_MSBITS6 = AC_FLOW_WALL[air_flow];
  }

  int AC_MSBITS7 = (AC_MSBITS3 + AC_MSBITS4 + AC_MSBITS5 + AC_MSBITS6) & B00001111;

  AC_CODE_TO_SEND =  AC_MSBITS1 << 4 ;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS2) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS3) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS4) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS5) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS6) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS7);

  ac_send_code(AC_CODE_TO_SEND);

  AC_POWER_ON = 1;
  AC_TEMPERATURE = temperature;
  AC_FLOW = air_flow;
}


void ac_change_air_swing(int air_swing) {
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

  ac_send_code(AC_CODE_TO_SEND);
}

void ac_power_down() {
  AC_CODE_TO_SEND = 0x88C0051;

  ac_send_code(AC_CODE_TO_SEND);

  AC_POWER_ON = 0;
}

void ac_air_clean(int air_clean) {
  if ( air_clean == 1) {
    AC_CODE_TO_SEND = 0x88C000C;
  } else {
    AC_CODE_TO_SEND = 0x88C0084;
  }

  ac_send_code(AC_CODE_TO_SEND);

  AC_AIR_ACLEAN = air_clean;
}

void setup()
{
  pinMode(GND_FOR_IR, OUTPUT);
  digitalWrite(GND_FOR_IR, LOW);
  irsend.begin();
  Serial.begin(115200);
  while (!Serial) {
   ; // wait for serial port to connect.
  }
  Serial.println();
}

void loop() {
    Serial.println("  - - - T E S T - - -   ");
    ac_activate(25, 1);
    delay(10000);
    ac_activate(27, 2);
    delay(10000);
    ac_power_down();
    delay(10000); 
}
