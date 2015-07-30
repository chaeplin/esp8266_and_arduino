#include <IRremote.h>
#include <Wire.h>

IRsend irsend;

/*
 Standalone Sketch to use with a Arduino UNO and a
 Sharp Optical Dust Sensor GP2Y1010AU0F
*/

// ------------------------------------------
int measurePin = A6; //Connect dust sensor to Arduino A6 pin
int ledPower = 2;   //Connect 3 led driver pins of dust sensor to Arduino D2

int IR_receive_recv_PIN = 6;
int IR_receive_GND_PIN  = 7;
int IR_receive_VCC_PIN  = 8;


// A6  : DUST IN
// D2  : DUST OUT
//
// D3  : IR OUT
//
// D6  : IR IN
// D7  : OUT GND
// D8  : OUT VCC
//
// D12 : OUT GND

IRrecv irrecv(IR_receive_recv_PIN); // Receive on pin 6

// ------------------------------------------
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

int voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

// -----------------------------------------
long startMills;

// ------------------------------------------
// IR
const int AC_TYPE  = 0;
// 0 : TOWER
// 1 : WALL

int AC_POWER_ON    = 0;
// 0 : off
// 1 : on

int AC_AIR_ACLEAN  = 0;
// 0 : off
// 1 : on --> power on

int AC_TEMPERATURE = 27;
// temperature : 18 ~ 30

int AC_FLOW        = 1;
// 0 : low
// 1 : mid
// 2 : high
// 3 : rotate

// IR
const int AC_FLOW_TOWER[4] = {0, 4, 6, 12};
const int AC_FLOW_WALL[4]  = {0, 2, 4, 5};

unsigned long AC_CODE_TO_SEND;

int r = LOW;
int o_r = LOW;

byte a, b;

// IR

// ------------------------------------------

// IR
void ac_send_code(unsigned long code)
{
  Serial.print("code to send : ");
  Serial.print(code, BIN);
  Serial.print(" : ");
  Serial.println(code, HEX);

  irsend.sendLGAC(code, 28);
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

  ac_send_code(AC_CODE_TO_SEND);
}

void ac_power_down()
{
  AC_CODE_TO_SEND = 0x88C0051;

  ac_send_code(AC_CODE_TO_SEND);

  AC_POWER_ON = 0;
}

void ac_air_clean(int air_clean)
{
  if ( air_clean == 1) {
    AC_CODE_TO_SEND = 0x88C000C;
  } else {
    AC_CODE_TO_SEND = 0x88C0084;
  }

  ac_send_code(AC_CODE_TO_SEND);

  AC_AIR_ACLEAN = air_clean;
}

// IR
long dumpInfo (decode_results *results)
{
   // Check if the buffer overflowed
  if (results->overflow) {
    Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWLEN");
    return 0;
  }
  
  if ( results->bits > 0 && results->bits == 32 ) {
    return results->value;;
  }
}



void setup()
{
  Serial.begin(38400);
  startMills = millis();
  Serial.println("Starting dust Sensor");
  pinMode(ledPower, OUTPUT);

  pinMode(IR_receive_VCC_PIN, OUTPUT);
  pinMode(IR_receive_GND_PIN, OUTPUT);
  digitalWrite(IR_receive_VCC_PIN, HIGH);
  digitalWrite(IR_receive_GND_PIN, LOW);

  irrecv.enableIRIn(); // Start the receiver

  Wire.begin(2);                // join i2c bus with address #2
  Wire.onRequest(requestEvent); // register event
  Wire.onReceive(receiveEvent);

}

void loop()
{
  decode_results results;
  
  if ( (millis() - startMills) > 1000 ) {

    digitalWrite(ledPower, LOW); // power on the LED
    delayMicroseconds(samplingTime);

    voMeasured = analogRead(measurePin); // read the dust value

    delayMicroseconds(deltaTime);
    digitalWrite(ledPower, HIGH); // turn the LED off
    delayMicroseconds(sleepTime);

    startMills = millis();

  }

  if (irrecv.decode(&results)) {
    Serial.println(dumpInfo(&results), HEX);
    delay(50);
    irrecv.resume(); // Continue receiving
  }
  
  // ------------------------------------------
  /* test
    ac_activate(25, 1);
    delay(5000);
    ac_activate(27, 0);
    delay(5000);
  */

  if ( r != o_r) {

    /*
    # a : mode or temp    b : air_flow, temp, swing, clean
    # 18 ~ 30 : temp      0 ~ 3 : flow // on
    # 0 : off             0
    # 1 : on              0
    # 2 : air_swing       0 or 1
    # 3 : air_clean       0 or 1
    # 4 : air_flow        0 ~ 3 : flow
    # 5 : temp            18 ~ 30
    # + : temp + 1
    # - : temp - 1
    # m : change cooling to air clean, air clean to cooling
    */
    Serial.print("a : ");
    Serial.print(a);
    Serial.print("  b : ");
    Serial.println(b);

    switch (a) {
      case 0: // off
        ac_power_down();
        break;
      case 1: // on
        ac_activate(AC_TEMPERATURE, AC_FLOW);
        break;
      case 2:
        if ( b == 0 | b == 1 ) {
          ac_change_air_swing(b);
        }
        break;
      case 3: // 1  : clean on, power on
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
      case '+':
        if ( 18 <= AC_TEMPERATURE && AC_TEMPERATURE <= 29 ) {
          ac_activate((AC_TEMPERATURE + 1), AC_FLOW);
        }
        break;
      case '-':
        if ( 19 <= AC_TEMPERATURE && AC_TEMPERATURE <= 30 ) {
          ac_activate((AC_TEMPERATURE - 1), AC_FLOW);
        }
        break;
      case 'm':
        /*
          if ac is on,  1) turn off, 2) turn on ac_air_clean(1)
          if ac is off, 1) turn on,  2) turn off ac_air_clean(0)
        */
        if ( AC_POWER_ON == 1 ) {
          ac_power_down();
          delay(100);
          ac_air_clean(1);
        } else {
          if ( AC_AIR_ACLEAN == 1) {
            ac_air_clean(0);
            delay(100);
          }
          ac_activate(AC_TEMPERATURE, AC_FLOW);
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

  
}

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

