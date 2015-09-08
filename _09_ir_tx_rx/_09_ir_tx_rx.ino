/* bugs
*  001 : initialise_number_select cause reset when called minno ~ maxno loop moret han 3 times
*/

#include <avr/eeprom.h>
#include <Wire.h>
#include <OneWire.h>
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>
#include "Timer.h"
#include <Time.h>


#define eeprom_read_to(dst_p, eeprom_field, dst_size) eeprom_read_block(dst_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(dst_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_read(dst, eeprom_field) eeprom_read_to(&dst, eeprom_field, sizeof(dst))
#define eeprom_write_from(src_p, eeprom_field, src_size) eeprom_write_block(src_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(src_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_write(src, eeprom_field) { typeof(src) x = src; eeprom_write_from(&x, eeprom_field, sizeof(x)); }
#define MIN(x,y) ( x > y ? y : x )

#define DEBUG_PRINT 1

// eeprom
// Change this any time the EEPROM content changes
const long magic_number = 0x807;

struct __eeprom_data {
  long magic;
  int pwrSrc;     // 1 : connected to TV, 0 : usb powered
  int startMode;  // if wrkMode == 1, 1 : auto start timer, pir, 0 : do nothing indicating a sign on the lcd
  int channelGap; // 1 ~ 5
  long tvOnTime;   // 30 ~ 90
};

// pins
#define SETUP_IN_PIN  12
#define IR_IN_PIN     10
#define DHTPIN         4
#define PIR_IN_PIN     2


// DHT22
#define DHTTYPE DHT22   // DHT 22  (AM2302)
DHT dht(DHTPIN, DHTTYPE, 15);

// LCD
LiquidCrystal_I2C lcd(0x27, 8, 2);

// IR
#define maxLen 800
volatile  unsigned int irBuffer[maxLen]; 
volatile unsigned int x = 0;

IRrecv irrecv(IR_IN_PIN);
IRsend irsend;

// Temperature
float tempCinside;
float Humidityinside;

// Timer
Timer t;
long temp_Mills;
long pir_Mills;

int tvIsOnEvent;

int irSendTvOutToCurChEvent ; 
int irSendTvOutToBlnkChEvent ;


// eeprom status
int o_pwrSrc;
int o_startMode;
int o_channelGap;
long o_tvOnTime;

// sw pin status
int setUpStatus;
int wrkModeStatus;
int backlightStatus;

// tv power status
int tvPowerStatus;

// Timer status
int timerStatus = 0;

// channel input selection delay ( smart tv need this)
long channelselectiondelay = 3000;

// tv IR code
unsigned long tv_input = 0x20DFD02F;
unsigned long tv_right = 0x20DF609F;
unsigned long tv_left  = 0x20DFE01F;
unsigned long tv_enter = 0x20DF22DD;
unsigned long tv_onoff = 0x20DF10EF;

//
int r   = 0;
int o_r = 0;

// PIR
volatile int pirOnOff = LOW;
int o_pirOnOff        = LOW;

// lcd icon
byte termometru[8] =
{
  B00100,
  B01010,
  B01010,
  B01110,
  B01110,
  B11111,
  B11111,
  B01110,
};

byte picatura[8] =
{
  B00100,
  B00100,
  B01010,
  B01010,
  B10001,
  B10001,
  B10001,
  B01110,
};

byte tvicon[8] =
{
  B10001,
  B01010,
  B00100,
  B11111,
  B10001,
  B10001,
  B10001,
  B11111,
};

byte timericon[8] =
{
  B01110,
  B10101,
  B10101,
  B10101,
  B10101,
  B10011,
  B10001,
  B01110,
};

byte powericon[8] =
{
  B11111,
  B11011,
  B10001,
  B11011,
  B11111,
  B11000,
  B11000,
  B11000,
};

byte piricon[8] =
{
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
};

// to reset after setup
void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup()
{
  // start serial
  if (DEBUG_PRINT) {
    Serial.begin (38400);
    Serial.println();
    Serial.println("TV controller Starting");
  }
  delay(20);

  // lcd
  lcd.init();
  //lcd.backlight();
  lcd.clear();

  delay(1000);

  // Timer start
  temp_Mills = millis();
  //pir_Mills = millis();

  // ir
  irrecv.enableIRIn();

  // pin mode
  pinMode(SETUP_IN_PIN, INPUT_PULLUP);
  pinMode(PIR_IN_PIN, INPUT);

  delay(2000);

  // PIR
  if ( digitalRead(PIR_IN_PIN) == 0 ) {
    attachInterrupt(0, PIRCHECKING, CHANGE);
    lcd.setCursor(7, 0);
    lcd.print("+");
  } else {
    attachInterrupt(0, remove_poweron_error, FALLING);
    pirOnOff = HIGH;
    o_pirOnOff = HIGH;
    lcd.setCursor(7, 0);
    lcd.print("-");
  }

  // read pin status
  setUpStatus   = digitalRead(SETUP_IN_PIN);

  lcd.backlight();
  
  // eeprom read
  long o_magic;
  eeprom_read(o_magic, magic);

  if (o_magic != magic_number  || setUpStatus == 0 ) {
    run_initialise_setup();
    resetFunc();
  }

  eeprom_read(o_pwrSrc, pwrSrc);
  eeprom_read(o_startMode, startMode);
  eeprom_read(o_channelGap, channelGap);
  eeprom_read(o_tvOnTime, tvOnTime);

//  o_tvOnTime  = 2;

  // lcd
  lcd.createChar(1, termometru);
  lcd.setCursor(0, 0);
  lcd.write(1);

  lcd.setCursor(6, 0);
  lcd.print((char)223);

  lcd.createChar(2, tvicon);
  lcd.createChar(3, beepicon);
  lcd.createChar(4, timericon);
  lcd.createChar(5, powericon);
  lcd.createChar(6, piricon);

  if ( wrkModeStatus == 1 && o_startMode == 1 ) {
    lcd.setCursor(0, 1);
    lcd.write(2);

    // if powered by TV
    if ( o_pwrSrc == 1 ) {

      // input change
      tvPowerStatus = 1;
      timerStatus = 1;

      tvOnTimer();

      lcd.setCursor(4, 1);
      lcd.write(4);

      lcd.setCursor(6, 1);
      lcd.write(5);

    } else {
      timerStatus = 0 ;
    }
  }

  // event Timer
  int updateTempCEvent      = t.every(1000, doUpdateTempC);
  int updateTempCArrayEvent = t.every(300000, doUpdateTempCArray);

}


// PIR
void remove_poweron_error()
{
  detachInterrupt(0);
  attachInterrupt(0, PIRCHECKING, CHANGE);
  pirOnOff = digitalRead(PIR_IN_PIN);
  //r = !r;
}

void loop()
{

  t.update();

  // receiving ir and change status
  // remote on / off, tv remote on / off
  decode_results results;

  // PIR
  if ( pirOnOff != o_pirOnOff) {
    if ( pirOnOff == 1 ) {
      doTvControlbyPir(0);
    } else {
      doTvControlbyPir(1);
    }
    o_pirOnOff = pirOnOff;
  }

  // IR receive
  if (irrecv.decode(&results)) {
    changemodebyir(&results);
    irrecv.enableIRIn();
  }

  if (r != o_r) {
    changelcdicon();
    alarm_set();
    o_r = r;
  }

}

void changelcdicon()
{

  if ( wrkModeStatus == 1 && o_startMode == 1 ) {
    lcd.setCursor(0, 1);
    lcd.write(2);

    if ( o_beepMode == 1 ) {
      lcd.setCursor(2, 1);
      lcd.write(3);
    }

    if ( timerStatus == 1 ) {
      lcd.setCursor(4, 1);
      lcd.write(4);
    } else {
      lcd.setCursor(4, 1);
      lcd.print(" ");
    }

  } else {
    lcd.setCursor(0, 1);
    lcd.print("     ");
  }

  if ( tvPowerStatus == 1 ) {
    lcd.setCursor(6, 1);
    lcd.write(5);
  } else {
    lcd.setCursor(6, 1);
    lcd.print(" ");
  }
}

// need to add timer reset, power status
void changemodebyir (decode_results *results)
{
  if ( results->bits > 0 && results->bits == 32 ) {
    switch (results->value) {
      case 0xFF02FD: // remote on
        if ( wrkModeStatus == 1 ) {
          if ( timerStatus == 1 ) {
            t.stop(tvIsOnEvent);
          }
          //timerStatus = 0;
        } else {
          temp_Mills = millis();
          //timerStatus = 1;
        }
        wrkModeStatus = ! wrkModeStatus;
        o_startMode = 1;
        r = !r;
        break;
      case 0xFF9867: // remote off
        if ( timerStatus == 1 ) {
          t.stop(tvIsOnEvent);
        } else {
          temp_Mills = millis();
          tvOnTimer();
        }
        timerStatus = ! timerStatus;
        r = !r;
        break;
      case 0x20DF10EF: // tv remote on/off
        if ( tvPowerStatus == 1 ) {
          t.stop(tvIsOnEvent);
          timerStatus = 0;
        } else {
          temp_Mills = millis();
          timerStatus = 1;
          tvOnTimer();
        }
        tvPowerStatus = ! tvPowerStatus;
        r = !r;
        break;
    }
  }
  return;
}

void PIRCHECKING()
{
  if (( millis() - pir_Mills ) < 600 ) {
    return;
  } else {

    pirOnOff = digitalRead(PIR_IN_PIN);

    pir_Mills = millis();
  }
}

void tvOnTimer()
{

  long time_o_tvOnTime = ( o_tvOnTime * 1000 ) * 60 ;
  tvIsOnEvent = t.after(time_o_tvOnTime , doTvOffTimer);

  if (DEBUG_PRINT) {
    //Serial.println(time_o_tvOnTime);
    Serial.print("tvOnTimer called : ");
    Serial.println(tvIsOnEvent);
  }
}


void doTvOffTimer()
{
  if (DEBUG_PRINT) {
    Serial.println("doTvOffTimer called");
  }
  if ( tvPowerStatus == 1) {
    tvPowerStatus = 0 ;
    t.stop(tvIsOnEvent);
    irSendTvOutbytimer(0);
  }
}

//
void doTvControlbyPir(int onoff)
{

  switch (onoff) {
    case 1:
      lcd.setCursor(15, 0);
      lcd.print("+");
      irSendTvOutbypir(0);
      break;
    case 0:
      lcd.setCursor(15, 0);
      lcd.write(6);
      irSendTvOutbypir(1);
      break;
  }

  r = !r;
}

void irSendTvOutToBlnkCh()
{
  for ( int i = 0 ; i < o_channelGap ; i++ ) {
    irsend.sendNEC(tv_right, 32);
    delay(100);
  }
  irsend.sendNEC(tv_enter, 32);
  delay(100);
  irrecv.enableIRIn();
}

void irSendTvOutToCurCh()
{
  for ( int i = 0 ; i < o_channelGap ; i++ ) {
    irsend.sendNEC(tv_left, 32);
    delay(100);
  }
  irsend.sendNEC(tv_enter, 32);
  delay(100);
  irrecv.enableIRIn();
}



void irSendTvOutbypir(int a)
{

  if (DEBUG_PRINT) {
    Serial.println("IRSend PIR input change called");
  }
  // need to update, remove delay, use timer.
  if ( wrkModeStatus == 1 && o_startMode == 1 && tvPowerStatus == 1) {
    switch (a) {
      case 0:
        irsend.sendNEC(tv_input, 32);
        irSendTvOutToCurChEvent = t.after(channelselectiondelay, irSendTvOutToCurCh);
        break;
      case 1:
        irsend.sendNEC(tv_input, 32);
        irSendTvOutToBlnkChEvent = t.after(channelselectiondelay, irSendTvOutToBlnkCh);
        break;
    }

    irrecv.enableIRIn();
  }
}

//
void irSendTvOutbytimer(int a)
{
  if (DEBUG_PRINT) {
    Serial.println("IRSend TV off called");
  }
  irsend.sendNEC(tv_onoff, 32);
  delay(300);
  irrecv.enableIRIn();
  r = !r;
}

// TimeTime
void displaytimeleft() {

  long time_o_tvOnTime = ( o_tvOnTime * 1000 ) * 60 ;
  long timeleft =  (time_o_tvOnTime - (millis() - temp_Mills) ) / 1000 ;

  if (DEBUG_PRINT) {
    Serial.print("time_o_tvOnTime : ");
    Serial.print(time_o_tvOnTime);
    Serial.print(" timeleft : ");
    Serial.print(timeleft);
    Serial.print(" tvIsOnEvent : ");
    Serial.println(tvIsOnEvent);
  }

  String str_a = String(long(timeleft));
  int length_a = str_a.length();

  lcd.setCursor(8, 1);
  if ( timerStatus == 1 &&  wrkModeStatus == 1 && o_startMode == 1 && tvPowerStatus == 1) {
    for ( int i = 0; i < ( 7 - length_a ) ; i++ ) {
      lcd.print(" ");
    }
    lcd.print(str_a);
  } else {
    lcd.print("       ");
  }

}

// update every 2 sec
void doUpdateTempC()
{
  tempCinside = getdalastemp();
  displayTemperature(tempCinside);
  displaytimeleft();
}

// update every 5 mins
void doUpdateTempCArray()
{
  for ( int i = 0 ; i < 11 ; i++ ) {
    tempCprevious[i] = tempCprevious[i + 1];
  }
  tempCprevious[11] = tempCinside;
}

void displayTemperature(float Temperature)
{
  lcd.setCursor(1, 0);
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ ) {
    lcd.print(" ");
  }

  lcd.print(Temperature, 1);

  if ( ( second() % 2 ) == 0 ) {
    lcd.setCursor(15, 1);
    lcd.print(".");
  } else {
    lcd.setCursor(15, 1);
    lcd.print(" ");
  }

  float o_tempCprevious ;
  if ( o_pwrSrc == 1 ) {
    o_tempCprevious = tempCprevious[10];
  } else {
    o_tempCprevious = tempCprevious[0];
  }

  if ( o_tempCprevious != 100 ) {
    float tempdiff = tempCinside - o_tempCprevious;

    lcd.setCursor(8, 0);
    if ( tempdiff >= 0 ) {
      lcd.print(" + ");
    } else {
      lcd.print(" - ");
    }

    String str_tempdiff = String(int abs(tempdiff));
    int length_tempdiff = str_tempdiff.length();

    lcd.setCursor(9, 0);
    lcd.print(abs(tempdiff), 1);
    if ( length_tempdiff == 1) {
      lcd.print(" ");
    }
  }
}

float getdalastemp()
{
  sensors.requestTemperatures();
  float tempC  = sensors.getTempC(insideThermometer);

  return tempC;
}

void alarm_set()
{
  /*
  if (( o_beepMode == 1) || (setUpStatus == 0) || (o_magic != magic_number ))  {
  digitalWrite(BZ_OU_PIN, LOW);
  delay(50);
  digitalWrite(BZ_OU_PIN, HIGH);
  }
  irrecv.enableIRIn();
  */
}


// eeprom
int initialise_boolean_select()
{
  decode_results results;
  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }

  switch (results.value) {
    case 0xFF02FD:
      return 1;
      break;
    case 0xFF9867:
      return 0;
      break;
    default:
      initialise_boolean_select();
      break;
  }
}

int initialise_number_select(int minno, int maxno, int curno, int chstep)
{

  lcd.setCursor(13, 1);
  lcd.print(curno);

  decode_results results;
  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  switch (results.value) {
    case 0xFF02FD:
      if ( curno >= maxno ) {
        curno = minno;
      } else {
        curno = curno + chstep ;
      }
      lcd.setCursor(13, 1);
      lcd.print(curno);
      initialise_number_select(minno, maxno, curno, chstep);
      break;
    case 0xFF9867:
      return curno;
      break;
    default:
      initialise_number_select(minno, maxno, curno, chstep);
      break;
  }
}


void run_initialise_setup() {

  decode_results results;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press any button");
  lcd.setCursor(0, 1);
  lcd.print("to start setup");
  lcd.setCursor(15, 1);
  lcd.print("*");


  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("power source");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : from TV");
  lcd.setCursor(0, 1);
  lcd.print("OFF: from other");

  o_pwrSrc = initialise_boolean_select();
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("start mode");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : Auto start");
  lcd.setCursor(0, 1);
  lcd.print("OFF: do nothing");

  o_startMode = initialise_boolean_select();
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("beep mode");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : beep on");
  lcd.setCursor(0, 1);
  lcd.print("OFF: beep off");

  o_beepMode = initialise_boolean_select();
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("channel gap");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : change no");
  lcd.setCursor(0, 1);
  lcd.print("OFF: done");

  o_channelGap = initialise_number_select(1, 5, 1, 1);
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select timer");
  lcd.setCursor(0, 1);
  lcd.print("for TV auto off");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : change no");
  lcd.setCursor(0, 1);
  lcd.print("OFF : done");

  o_tvOnTime = initialise_number_select(30, 90, 50, 5);
  alarm_set();

  int initialise_eeprom_done =  initialise_eeprom(o_pwrSrc, o_startMode, o_beepMode, o_channelGap, o_tvOnTime) ;

  while (initialise_eeprom_done != 1 ) { }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setup is done");
  lcd.setCursor(0, 1);
  if ( setUpStatus == 0 ) {
    lcd.print("change setup sw");
  }
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Press any button");
  lcd.setCursor(0, 1);
  lcd.print("to reset");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }

}


int initialise_eeprom(int o_pwrSrc, int o_startMode, int o_channelGap, long o_tvOnTime)
{
  eeprom_write(o_pwrSrc, pwrSrc);
  eeprom_write(o_startMode, startMode);
  eeprom_write(o_channelGap, channelGap);
  eeprom_write(o_tvOnTime, tvOnTime);
  eeprom_write(magic_number, magic);

  return 1;
}


