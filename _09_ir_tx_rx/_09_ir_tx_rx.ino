
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
  int tvOnTime;       // 30 ~ 120
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


void initialise_eeprom() 
{
  /*
  eeprom_write(0, first);
  eeprom_write(0, second);
  eeprom_write(magic_number, magic);
  */
}


boolean initialise_pwr_src_select()
{
  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  switch (results.value) {
    case 0xFF02FD:
        return True;
        break;
    case 0xFF9867:
        return False;
        break;
    default:
        initialise_pwr_src_select();
        break;
  }
}

void run_initialise_setup() {

  decode_results results;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Setup :");
  lcd.setCursor(0, 1);
  lcd.print("Press any button");

  irrecv.resume();
  while(irrecv.decode(&results) != 1 ) { } 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print('Select pwr src :');
  lcd.setCursor(0, 1);
  lcd.print("ON: TV OFF: USB"); 

  boolean pwrSrc = initialise_pwr_src_select();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(pwrSrc);


/*
  lcd.setCursor(0, 0);

  if ((magic != magic_number) || ( setUpStatus == 0 ) {

 
  lcd.print('0');

  boolean pwrSrc;     // True : connected to TV, False : usb powered
  boolean wrkMode;    // True : thermostat + TV, False : thermostat
  boolean startMode;  // if wrkMode == True, True : run TV on/off or channel change,
                      // False : do nothing indicating a sign on the lcd
  boolean beepMode;   // True : beep on
  boolean offMode;    // True : TV on/off, False : channel change
  int channelGap;     // 1 ~ 5
  int tvOnTime;       // 30 ~ 120
  int tvOffTime;      // 5 ~ 20


  decode_results results;

  if (irrecv.decode(&results)) {
    dumpInfo(&results);

  }

 */
  // initialise_eeprom(); 

}
