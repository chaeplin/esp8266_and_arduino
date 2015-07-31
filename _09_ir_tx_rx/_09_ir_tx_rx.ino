/*
A4, A5 - I2C

A2 - SETUP / IN
A0 - ENTER / IN
D12 - DOWN / IN
D11 - UP / IN
D10 - IR / IN
D9 - BUZZER /OUT
D4 - TEMP(DS18B20) /IN
D3 - IR / OUT
D2 - PIR /IN
*/




#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>

//
int SETUP_IN_PIN = A2;
//int ENTER_IN_PIN = A0;
int ENTER_IN_PIN = 13;
int UP_IN_PIN    = 12;
int DN_IN_PIN    = 11;

//
int IR_IN_PIN    = 10;
int PIR_IN_PIN   = 2;

int BZ_OU_PIN    =  9;

// LCD
LiquidCrystal_I2C lcd(0x27, 20, 4);

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
float tempCinside ;
long startMills;
long tv_startMills;

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

  // lcd
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.createChar(1, termometru);
  lcd.setCursor(0, 0);
  lcd.write(1);
  // lcd

  Serial.begin (38400);
  Serial.println("TV controller Starting");
  delay(20);

  irrecv.enableIRIn(); // Start the receiver

  startMills = millis();

  // pin mode

  pinMode(SETUP_IN_PIN, INPUT_PULLUP);
  pinMode(ENTER_IN_PIN, INPUT_PULLUP);
  pinMode(DN_IN_PIN, INPUT_PULLUP);
  pinMode(UP_IN_PIN, INPUT_PULLUP);
  pinMode(PIR_IN_PIN, INPUT_PULLUP);

  pinMode(BZ_OU_PIN, OUTPUT);
  digitalWrite(BZ_OU_PIN, HIGH);


  // temp
  sensors.begin();
  if (!sensors.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0");
  sensors.setResolution(insideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  tempCinside  = sensors.getTempC(insideThermometer);

  if ( isnan(tempCinside) ) {
    Serial.println("Failed to read from sensor!");
    return;
  }
  // temp

}


void loop()
{
  /*
   *
    pinMode(SETUP_IN_PIN, INPUT_PULLUP);
    pinMode(ENTER_IN_PIN, INPUT_PULLUP);
    pinMode(DN_IN_PIN, INPUT_PULLUP);
    pinMode(UP_IN_PIN, INPUT_PULLUP);
    pinMode(PIR_IN_PIN, INPUT_PULLUP);
   *
   */
  /*
    int x = digitalRead(ENTER_IN_PIN);
    Serial.println(x);
    //getdalastemp();
    //Serial.println(tempCinside);
    delay(1000);
  */

  decode_results results;

  if (irrecv.decode(&results)) {
    dumpInfo(&results);

  }

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


