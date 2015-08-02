/* bugs
*  001 : initialise_number_select cause reset when called minno ~ maxno loop moret han 3 times
*  002 : o_tempCinside is not a moving point
*/

// eeprom
#include <avr/eeprom.h>
#define eeprom_read_to(dst_p, eeprom_field, dst_size) eeprom_read_block(dst_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(dst_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_read(dst, eeprom_field) eeprom_read_to(&dst, eeprom_field, sizeof(dst))
#define eeprom_write_from(src_p, eeprom_field, src_size) eeprom_write_block(src_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(src_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_write(src, eeprom_field) { typeof(src) x = src; eeprom_write_from(&x, eeprom_field, sizeof(x)); }
#define MIN(x,y) ( x > y ? y : x )

#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

// Change this any time the EEPROM content changes
const long magic_number = 0x326;


struct __eeprom_data {
  long magic;
  int pwrSrc;     // 1 : connected to TV, 0 : usb powered
  int wrkMode;    // 1 : thermostat + TV, 0 : thermostat
  int startMode;  // if wrkMode == 1, 1 : run TV on/off or channel change, 0 : do nothing indicating a sign on the lcd
  int beepMode;   // 1 : beep on
  int offMode;    // 1 : TV on/off, 0 : input change
  int channelGap; // 1 ~ 5
  int tvOnTime;   // 30 ~ 90
  int tvOffTime;  // 5 ~ 20
};

//
int SETUP_IN_PIN = A2;
int WRK_MODE_IN_PIN = A0;
/*
int UP_IN_PIN    = 12;
int DN_IN_PIN    = 11;
*/

int IR_IN_PIN    = 10;
int PIR_IN_PIN   = 2;
int BZ_OU_PIN    = 9;

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// IR
#define maxLen 800
volatile  unsigned int irBuffer[maxLen]; //stores timings - volatile because changed by ISR
volatile unsigned int x = 0; //Pointer thru irBuffer - volatile because changed by ISR

IRrecv irrecv(IR_IN_PIN);
IRsend irsend;

// DS18B20
#define ONE_WIRE_BUS 4
#define TEMPERATURE_PRECISION 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress insideThermometer;

// Temperature
float tempCinside;
float o_tempCinside;
float c_tempCinside;

// timer
long startMills;
long tv_off_Mills;
long tv_on_Mills;
long tempMills;
long o_tempMills;
long pir_Mills;

// temp
int o_tempminpassed = 0;

// sw status
int setUpStatus;
int wrkModeStatus;
int tvPowerStatus = 0;
int timerStatus   = 0;
int timerOnOff    = 0;

volatile int pirOnOff      = 0;
int o_pirOnOff    = 0;

int r   = LOW;
int o_r = LOW;
int t   = LOW;
int o_t = LOW;

// eeprom status
int o_pwrSrc;
int o_wrkMode;
int o_startMode;
int o_beepMode;
int o_offMode;
int o_channelGap;
int o_tvOnTime;
int o_tvOffTime;

// tv
unsigned long tv_input = 0x20DFD02F;
unsigned long tv_right = 0x20DF609F;
unsigned long tv_left  = 0x20DFE01F;
unsigned long tv_enter = 0x20DF22DD;
unsigned long tv_onoff = 0x20DF10EF;

// lcd
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

byte beepicon[8] =
{
  B00000,
  B11111,
  B10001,
  B10101,
  B10101,
  B10101,
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

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup()
{
  // start serial
  Serial.begin (38400);
  Serial.println();
  Serial.println("TV controller Starting");
  delay(20);

  // Timer start
  startMills = millis();
  tempMills = millis();
  o_tempMills = millis();
  tv_off_Mills = millis();
  tv_on_Mills = millis();

  // lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // ir
  irrecv.enableIRIn();

  // pin mode
  pinMode(SETUP_IN_PIN, INPUT);
  pinMode(WRK_MODE_IN_PIN, INPUT);
  /*
  pinMode(DN_IN_PIN, INPUT_PULLUP);
  pinMode(UP_IN_PIN, INPUT_PULLUP);
  */
  pinMode(PIR_IN_PIN, INPUT);

  // buzzer
  pinMode(BZ_OU_PIN, OUTPUT);
  digitalWrite(BZ_OU_PIN, HIGH);
  digitalWrite(SETUP_IN_PIN, HIGH);
  digitalWrite(WRK_MODE_IN_PIN, HIGH);

  delay(200);

  // eeprom read
  long o_magic;

  eeprom_read(o_magic, magic);
  eeprom_read(o_pwrSrc, pwrSrc);
  eeprom_read(o_wrkMode, wrkMode);
  eeprom_read(o_startMode, startMode);
  eeprom_read(o_beepMode, beepMode);
  eeprom_read(o_offMode, offMode);
  eeprom_read(o_channelGap, channelGap);
  eeprom_read(o_tvOnTime, tvOnTime);
  eeprom_read(o_tvOffTime, tvOffTime);

  // read pin status
  setUpStatus   = digitalRead(SETUP_IN_PIN);
  wrkModeStatus = digitalRead(WRK_MODE_IN_PIN);

  if (o_magic != magic_number ) {
    run_initialise_setup();
    resetFunc();
  }

  if ( setUpStatus == 0 ) {
    run_initialise_setup();
    resetFunc();
  }

  if ( wrkModeStatus == 0 ) {
    o_wrkMode = wrkModeStatus;
  }

  if ( o_pwrSrc == 1) {
    o_offMode = 0;
  }


  // temp sensor
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  tempCinside   = sensors.getTempC(insideThermometer);
  c_tempCinside = tempCinside;

  if ( isnan(tempCinside) ) {
    Serial.println("Failed to read from sensor!");
    return;
  }

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

  if ( o_wrkMode == 1 && o_startMode == 1 ) {
    lcd.setCursor(0, 1);
    lcd.write(2);

    if ( o_beepMode == 1 ) {
      lcd.setCursor(2, 1);
      lcd.write(3);
    }

    lcd.setCursor(4, 1);
    lcd.write(4);
  }

  attachInterrupt(0, PIRCHECKING, CHANGE);
}

void PIRCHECKING()
{
  pirOnOff = digitalRead(PIR_IN_PIN);
}

void loop()
{
  // receiving ir and change status
  // remote on / off, tv remote on / off
  decode_results results;

  if (irrecv.decode(&results)) {
    changemodebyir(&results);
    irrecv.enableIRIn();
  }

  if ((millis() - tempMills) >= 2000 ) {
    getdalastemp();
    displayTemperature();
    tempMills = millis();

    if (timerOnOff == 0) {
      displaytimeleft(o_tvOnTime - ((millis() - tv_off_Mills) / 1000 / 60) );
    } else {
      displaytimeleft(o_tvOffTime - ((millis() - tv_on_Mills) / 1000 / 60) );
    }
    
    if ( (millis() - o_tempMills) >= 600000 ) {
      Serial.println("o_tempCinside called");
      o_tempCinside = c_tempCinside;
      c_tempCinside = tempCinside;
      o_tempminpassed = 1;
      o_tempMills = millis();
    }
  }

  if ((millis() - tv_off_Mills) >= ( o_tvOnTime * 60 * 1000 ) && (timerOnOff == 0) && ( o_pirOnOff != 1) ) {
    Serial.println("Timer off called");
    turn_onoff_tv(1);
  }

  if ((millis() - tv_on_Mills) >= ( o_tvOffTime * 60 * 1000 ) && (timerOnOff == 1) && ( o_pirOnOff != 1)) {
    Serial.println("Timer on called");
    turn_onoff_tv(0);
  }

  if (t != o_t) {
    tv_off_Mills = millis();
    o_t = t;
  }

  if (r != o_r) {
    changelcdicon();
    alarm_set();
    o_r = r;
  }


  if ( pirOnOff != o_pirOnOff ) {
    if (pirOnOff == 1) {
      Serial.println("PIR rising called");
      o_pirOnOff = pirOnOff;
      lcd.setCursor(15, 0);
      lcd.write(6);
      turn_onoff_tv(1);
      delay(100);
    } else {
      Serial.println("PIR timer called");
      o_pirOnOff = pirOnOff;
      lcd.setCursor(15, 0);
      lcd.print(" ");
      if ( timerOnOff == 1) {
        turn_onoff_tv(0);
      }
      delay(100);
    }
  }
  delay(100);
}

void displaytimeleft(float a) {
  String str_a = String(int(a));
  int length_a = str_a.length();

  lcd.setCursor(10, 1);
  for ( int i = 0; i < ( 3 - length_a ) ; i++ ) {
    lcd.print(" ");
  }
  lcd.print(str_a);
}

void turn_onoff_tv(int a)
{
  if ( o_wrkMode == 1 && o_startMode == 1 ) {
    if ( o_pwrSrc == 0 ) {
        if ( a == 1 && tvPowerStatus == 1) {
          irSendTv(0);
          tv_on_Mills = millis();
          timerOnOff = 1;
          r = !r;
        }

        if ( a == 0 && tvPowerStatus == 0) {
          irSendTv(1);
          tv_off_Mills = millis();
          timerOnOff = 0;
          r = !r;
        }
    } else {
        if ( a == 1 ) {
          irSendTv(0);
          tv_on_Mills = millis();
          timerOnOff = 1;
          r = !r;
        }

        if ( a == 0 ) {
          irSendTv(1);
          tv_off_Mills = millis();
          timerOnOff = 0;
          r = !r;
        }      
    }

  }
}

void irSendTv(int a)
{
  if ( o_offMode == 1 ) {
    Serial.println("IRSend on/off called");
    irsend.sendNEC(tv_onoff, 32);
    tvPowerStatus = !tvPowerStatus ;
    delay(300);
    irrecv.enableIRIn();
  } else {
    Serial.println("IRSend CH change called");
    if ( a == 1) {
      irsend.sendNEC(tv_input, 32);
      delay(3000);
      for ( int i = 0 ; i < o_channelGap ; i++ ) {
        irsend.sendNEC(tv_left, 32);
        delay(300);
      }
      irsend.sendNEC(tv_enter, 32);
    } else {
      irsend.sendNEC(tv_input, 32);
      delay(3000);
      for ( int i = 0 ; i < o_channelGap ; i++ ) {
        irsend.sendNEC(tv_right, 32);
        delay(300);
      }
      irsend.sendNEC(tv_enter, 32);
    }
    irrecv.enableIRIn();
  }
}


void changelcdicon()
{

  if ( o_wrkMode == 1 && o_startMode == 1 ) {
    lcd.setCursor(0, 1);
    lcd.write(2);

    if ( o_beepMode == 1 ) {
      lcd.setCursor(2, 1);
      lcd.write(3);
    }

    if ( timerStatus == 0 ) {
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
  irrecv.enableIRIn();
}

void changemodebyir (decode_results *results)
{
  if ( results->bits > 0 && results->bits == 32 ) {
    switch (results->value) {
      case 0xFF02FD: // remote on
        o_wrkMode = ! o_wrkMode;
        o_startMode = 1;
        r = !r;
        break;
      case 0xFF9867: // remote off
        timerStatus = ! timerStatus;
        r = !r;
        t = !t;
        break;
      case 0x20DF10EF: // tv remore on/off
        tvPowerStatus = ! tvPowerStatus ;
        r = !r;
        break;
    }
  }
}

void displayTemperaturedigit(float Temperature)
{
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ ) {
    lcd.print(" ");
  }

  lcd.print(Temperature, 1);
}


void displayTemperature()
{
  lcd.setCursor(1, 0);
  displayTemperaturedigit(tempCinside);

  if ( o_tempminpassed == 0 ) {
    return;
  } else {
    float tempdiff = tempCinside - o_tempCinside;

    lcd.setCursor(8, 0);
    if ( tempdiff >= 0 ) {
      lcd.print("+");
    } else {
      lcd.print("-");
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

void getdalastemp()
{
  sensors.requestTemperatures();
  tempCinside  = sensors.getTempC(insideThermometer);

  if ( isnan(tempCinside) ) {
    Serial.println("Failed to read from sensor!");
    return;
  }
}

void alarm_set()
{
  if ( o_beepMode == 1) {
    digitalWrite(BZ_OU_PIN, LOW);
    delay(50);
    digitalWrite(BZ_OU_PIN, HIGH);
  }
  irrecv.enableIRIn();
}

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

  int pwrSrc = initialise_boolean_select();
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("working mode");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : TV + thermo");
  lcd.setCursor(0, 1);
  lcd.print("OFF: thermo");

  int wrkMode = initialise_boolean_select();
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

  int startMode = initialise_boolean_select();
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

  int beepMode = initialise_boolean_select();
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("TV off mode");
  lcd.setCursor(15, 1);
  lcd.print("*");

  irrecv.resume();
  while (irrecv.decode(&results) != 1 ) { }
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : TV on/off");
  lcd.setCursor(0, 1);
  lcd.print("OFF: IN change");

  int offMode = initialise_boolean_select();
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

  int channelGap = initialise_number_select(1, 5, 1, 1);
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
  lcd.print("OFF: done");

  int tvOnTime = initialise_number_select(30, 90, 50, 5);
  alarm_set();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select timer");
  lcd.setCursor(0, 1);
  lcd.print("for TV auto on");
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

  int tvOffTime = initialise_number_select(5, 20, 10, 5);
  alarm_set();

  //
  int initialise_eeprom_done =  initialise_eeprom(pwrSrc, wrkMode, startMode, beepMode, offMode, channelGap, tvOnTime, tvOffTime) ;

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
  alarm_set();
  alarm_set();
  alarm_set();
}


int initialise_eeprom(int i_pwrSrc, int i_wrkMode, int i_startMode, int i_beepMode, int i_offMode, int i_channelGap, int i_tvOnTime, int i_tvOffTime)
{
  eeprom_write(i_pwrSrc, pwrSrc);
  eeprom_write(i_wrkMode, wrkMode);
  eeprom_write(i_startMode, startMode);
  eeprom_write(i_beepMode, beepMode);
  eeprom_write(i_offMode, offMode);
  eeprom_write(i_channelGap, channelGap);
  eeprom_write(i_tvOnTime, tvOnTime);
  eeprom_write(i_tvOffTime, tvOffTime);
  eeprom_write(magic_number, magic);

  alarm_set();
  alarm_set();

  return 1;
}

/*
*
*/
