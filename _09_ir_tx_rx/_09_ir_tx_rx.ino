
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

// eeprom
#include <avr/eeprom.h>
#define eeprom_read_to(dst_p, eeprom_field, dst_size) eeprom_read_block(dst_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(dst_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_read(dst, eeprom_field) eeprom_read_to(&dst, eeprom_field, sizeof(dst))
#define eeprom_write_from(src_p, eeprom_field, src_size) eeprom_write_block(src_p, (void *)offsetof(__eeprom_data, eeprom_field), MIN(src_size, sizeof((__eeprom_data*)0)->eeprom_field))
#define eeprom_write(src, eeprom_field) { typeof(src) x = src; eeprom_write_from(&x, eeprom_field, sizeof(x)); }
#define MIN(x,y) (x > y ? y : x)

// Change this any time the EEPROM content changes
const long magic_number = 0x0326;
 
struct __eeprom_data {
  long magic;
  boolean pwrSrc;     // True : connected to TV, False : usb powered
  boolean wrkMode;    // True : thermostat + TV, False : thermostat
  boolean startMode;  // if wrkMode == True, True : run TV on/off or channel change, False : do nothing indicating a sign on the lcd
  boolean beepMode;   // True : beep on
  boolean offMode;    // True : TV on/off, False : channel change
  int channelGap;     // 1 ~ 5
  int tvOnTime;       // 30 ~ 90
  int tvOffTime;      // 5 ~ 20
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

//
float tempCinside;
long startMills;
long tv_off_Mills;
int setUpStatus;
int wrkModeStatus;

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

void(* resetFunc) (void) = 0; //declare reset function @ address 0

void setup()
{

  // start serial
  Serial.begin (38400);
  Serial.println("TV controller Starting");
  delay(20);

  // Timer start
  startMills = millis();

  // lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // ir
  irrecv.enableIRIn();

  // pin mode
  pinMode(SETUP_IN_PIN, INPUT_PULLUP);
  pinMode(WRK_MODE_IN_PIN, INPUT_PULLUP);
  /*
  pinMode(DN_IN_PIN, INPUT_PULLUP);
  pinMode(UP_IN_PIN, INPUT_PULLUP);
  */
  pinMode(PIR_IN_PIN, INPUT_PULLUP);

  // buzzer
  pinMode(BZ_OU_PIN, OUTPUT);
  digitalWrite(BZ_OU_PIN, HIGH);

  // read pin status
  setUpStatus   = digitalRead(SETUP_IN_PIN);
  wrkModeStatus = digitalRead(WRK_MODE_IN_PIN);

  // initialize eeprom  
  long magic;
  eeprom_read(magic, magic);
  if ((magic != magic_number) || ( setUpStatus == 0 )) {
     run_initialise_setup();
  }


  // temp sensor
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  tempCinside  = sensors.getTempC(insideThermometer);

  if ( isnan(tempCinside) ) {
    Serial.println("Failed to read from sensor!");
    return;
  }

  // lcd
  lcd.createChar(1, termometru);
  lcd.setCursor(0, 0);
  lcd.write(1);
  
}


void loop()
{



  /*
   *
    pinMode(SETUP_IN_PIN, INPUT_PULLUP);
    pinMode(WRK_MODE_IN_PIN, INPUT_PULLUP);
    pinMode(DN_IN_PIN, INPUT_PULLUP);
    pinMode(UP_IN_PIN, INPUT_PULLUP);
    pinMode(PIR_IN_PIN, INPUT_PULLUP);
   *
   */
  /*
    int x = digitalRead(WRK_MODE_IN_PIN);
    Serial.println(x);
    //getdalastemp();
    //Serial.println(tempCinside);
    delay(1000);
  */

/*
  decode_results results;

  if (irrecv.decode(&results)) {
    dumpInfo(&results);

  }
*/



}

void  dumpInfo (decode_results *results)
{
  // Check if the buffer overflowed
  if (results->overflow) {
    Serial.println("IR code too long. Edit IRremoteInt.h and increase RAWLEN");
    return;
  }

  if ( results->bits > 0 && results->bits == 32 ) {

    // Show Code & length
    Serial.print("Code      : ");
    Serial.println(results->value, HEX);
  }
  delay(50);
  irrecv.resume();
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
  Serial.println("===> alarm_set");

  digitalWrite(BZ_OU_PIN, LOW);
  delay(100);
  digitalWrite(BZ_OU_PIN, HIGH);
}

boolean initialise_boolean_select()
{
  decode_results results;
  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

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

boolean initialise_number_select(int minno, int maxno, int curno, int chstep)
{

  lcd.setCursor(13, 1);
  lcd.print(curno);
        
  decode_results results;
  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

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
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);
  lcd.print("power source");
  lcd.setCursor(15, 1);
  lcd.print("*");  

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ON : from TV");
  lcd.setCursor(0, 1);
  lcd.print("OFF: from other");    

  boolean pwrSrc = initialise_boolean_select();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);  
  lcd.print("working mode");
  lcd.setCursor(15, 1);
  lcd.print("*");  

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : TV + thermo");
  lcd.setCursor(0, 1);
  lcd.print("OFF: thermo");  

  boolean wrkMode = initialise_boolean_select();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);  
  lcd.print("start mode");
  lcd.setCursor(15, 1);
  lcd.print("*");  

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : Auto start");
  lcd.setCursor(0, 1);
  lcd.print("OFF: do nothing");  

  boolean startMode = initialise_boolean_select(); 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);  
  lcd.print("beep mode");
   lcd.setCursor(15, 1);
  lcd.print("*"); 

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : beep on");
  lcd.setCursor(0, 1);
  lcd.print("OFF: beep off");  

  boolean beepMode = initialise_boolean_select();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);  
  lcd.print("TV off mode");
  lcd.setCursor(15, 1);
  lcd.print("*");
  
  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : TV on/off");
  lcd.setCursor(0, 1);
  lcd.print("OFF: IN change");  

  boolean offMode = initialise_boolean_select();  

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select");
  lcd.setCursor(0, 1);  
  lcd.print("channel gap");
  lcd.setCursor(15, 1);
  lcd.print("*");
  
  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : change no");
  lcd.setCursor(0, 1);
  lcd.print("OFF: done");  

  int channelGap = initialise_number_select(1, 5, 1, 1);  

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select timer");
  lcd.setCursor(0, 1);  
  lcd.print("for TV auto off");
   lcd.setCursor(15, 1);
  lcd.print("*"); 

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : change no");
  lcd.setCursor(0, 1);
  lcd.print("OFF: done");  

  int tvOnTime = initialise_number_select(30, 90, 50, 5);  

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Select timer");
  lcd.setCursor(0, 1);  
  lcd.print("for TV auto on");
   lcd.setCursor(15, 1);
  lcd.print("*"); 

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("ON : change no");
  lcd.setCursor(0, 1);
  lcd.print("OFF: done");  

  int tvOffTime = initialise_number_select(5, 20, 10, 5);  

  //
  boolean initialise_eeprom_done =  initialise_eeprom(pwrSrc, wrkMode, startMode, beepMode, offMode, channelGap, tvOnTime, tvOffTime) ;

  while(initialise_eeprom_done != 1 ) { } 

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
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear(); 
  lcd.setCursor(0, 0);
  lcd.print("Press any button");
  lcd.setCursor(0, 1);
  lcd.print("to reset");

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { }       

  resetFunc(); 

}


boolean initialise_eeprom(boolean i_pwrSrc, boolean i_wrkMode, boolean i_startMode, boolean i_beepMode, boolean i_offMode, int i_channelGap, int i_tvOnTime, int i_tvOffTime) 
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

  return 1;
}



