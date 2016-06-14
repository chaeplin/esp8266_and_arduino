/* pins
  D9 : ir in
  D8 : dust ir out
  D4 : signal to esp
  D3 : ir out
  D2 : sqwe in
  A0 : dust in
  A1 : moisture in
*/

#include <avr/pgmspace.h>
#include <IRremote.h>
#include <SPI.h>
#include <Wire.h>
#include <Average.h>
#include <avr/pgmspace.h>
#include <TimeLib.h>
#include <RtcDS3231.h>
#include <LiquidCrystal_I2C.h>

#define IR_IN_PIN 9
#define DUST_OUT_PIN 8
#define ESP_DATA_PIN 4
#define IR_OUT_PIN 3
#define SQWV_PIN 2

#define DUST_IN_PIN A0
#define MOISTURE_PIN A1

#define dust_samplingTime 280
#define dust_deltaTime 40

const char termometru[8]       PROGMEM = { B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110 };
const char picatura[8]         PROGMEM = { B00100, B00100, B01010, B01010, B10001, B10001, B10001, B01110 };
const char dustDensityicon[8]  PROGMEM = { B11111, B11111, B11011, B10001, B10001, B11011, B11111, B11111 };
const char dustDensityfill[8]  PROGMEM = { B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111 };
const char pirfill[8]          PROGMEM = { B00111, B00111, B00111, B00111, B00111, B00111, B00111, B00111 };
const char powericon[8]        PROGMEM = { B11111, B11011, B10001, B11011, B11111, B11000, B11000, B11000 };
const char nemoicon[8]         PROGMEM = { B11011, B11011, B00100, B11111, B10101, B11111, B01010, B11011 };
const char callbackicon[8]     PROGMEM = { B11111, B11111, B11111, B11111, B00000, B00000, B00000, B00000 };

volatile bool balm_isr;
volatile bool bwrite;

int dustMeasured = 0;
int moistureMeasured = 0;
float dustDensity = 0;

typedef struct
{
  uint32_t hash;
  uint32_t timenow;
  uint16_t temperaturein;
  uint16_t temperatureout;
  uint16_t humidity;
  uint16_t powerall;
  uint16_t powerac;
  uint16_t nemoweight;
  uint16_t nano_dustDensity;
  uint16_t nano_moisture;
  uint8_t doorpir;
  uint8_t hostall;
  uint8_t hosttwo;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t nano_ac_in;
  uint8_t nano_pir_in;
} data;

data data_esp;
data data_nano;

volatile bool haveData = false;

/* ac */
const int AC_TYPE    = 0;     // 0 : TOWER, 1 : WALL
int AC_POWER_ON      = 0;     // 0 : off, 1 : on
int AC_AIR_ACLEAN    = 0;     // 0 : off,  1 : on --> power on
int AC_TEMPERATURE   = 27;    // temperature : 18 ~ 30
int AC_FLOW          = 0;     // 0 : low, 1 : mid , 2 : high
int AC_FLOW_TOWER[3] = {0, 4, 6};
int AC_FLOW_WALL[3]  = {0, 2, 4};

unsigned long AC_CODE_TO_SEND;

Average<float> ave1(10);
Average<float> ave2(10);
LiquidCrystal_I2C lcd(0x27, 20, 4);
RtcDS3231 Rtc;
IRrecv irrecv(IR_IN_PIN);
IRsend irsend;

/* for hash */
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length)
{
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data)
{
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

template <typename T> unsigned int SPI_writeAnything(const T& value)
{
  const byte * p = (const byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    SPI.transfer(*p++);
  return i;
}  // end of SPI_writeAnything

template <typename T> unsigned int SPI_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = SPI.transfer (0);
  return i;
}  // end of SPI_readAnything

template <typename T> unsigned int SPI_readAnything_ISR(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  *p++ = SPDR;  // get first byte
  for (i = 1; i < sizeof value; i++)
    *p++ = SPI.transfer (0);
  return i;
}  // end of SPI_readAnything_ISR

// SPI interrupt routine
ISR (SPI_STC_vect)
{
  SPI_readAnything_ISR (data_esp);
  haveData = true;
}

void alm_isr()
{
  balm_isr = true;
}

time_t requestSync()
{
  return 0;
}

time_t requestRtc()
{
  RtcDateTime Epoch32Time = Rtc.GetDateTime();
  return (Epoch32Time + 946684800);
}

void setup()
{
  data_nano.timenow          = 0;
  data_nano.temperaturein    = 0;
  data_nano.temperatureout   = 0;
  data_nano.humidity         = 0;
  data_nano.powerall         = 0;
  data_nano.powerac          = 0;
  data_nano.nemoweight       = 0;
  data_nano.doorpir          = 0;
  data_nano.hostall          = 0;
  data_nano.hosttwo          = 0;
  data_nano.accmd            = 0;
  data_nano.actemp           = 0;
  data_nano.acflow           = 0;
  data_nano.nano_dustDensity = 0;
  data_nano.nano_moisture    = 0;
  data_nano.nano_ac_in       = 0;
  data_nano.nano_pir_in      = 0;
  data_nano.hash = calc_hash(data_nano);

  pinMode(SQWV_PIN, INPUT_PULLUP);
  pinMode(DUST_OUT_PIN, OUTPUT);
  pinMode(ESP_DATA_PIN, OUTPUT);

  digitalWrite(DUST_OUT_PIN, HIGH);
  digitalWrite(ESP_DATA_PIN, HIGH);

  pinMode(MISO, OUTPUT);
  SPCR |= _BV(SPE);
  SPI.attachInterrupt();

  Serial.begin(115200);

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  balm_isr = false;
  bwrite   = false;

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.createChar(1, termometru);
  lcd.createChar(2, picatura);
  lcd.createChar(3, dustDensityicon);
  lcd.createChar(4, dustDensityfill);
  lcd.createChar(5, pirfill);
  lcd.createChar(6, powericon);
  lcd.createChar(7, nemoicon);
  lcd.createChar(8, callbackicon);

  lcd.setCursor(0, 1);
  lcd.write(1);

  lcd.setCursor(0, 2);
  lcd.write(2);

  lcd.setCursor(8, 2);  // power
  lcd.write(6);

  lcd.setCursor(0, 3);  // nemo
  lcd.write(7);

  lcd.setCursor(8, 3); // dust
  lcd.write(3);

  lcd.setCursor(6, 1);
  lcd.print((char)223);

  lcd.setCursor(12, 1);
  lcd.print((char)223);

  lcd.setCursor(6, 2);
  lcd.print("%");

  setSyncProvider(requestRtc);
  setSyncInterval(60);

  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.println();

  if (!Rtc.IsDateTimeValid())
  {
    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled)
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled)
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePinClockFrequency(DS3231SquareWaveClock_1Hz);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeClock);

  attachInterrupt(0, alm_isr, FALLING);
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  if (balm_isr)
  {
    digitalClockDisplay();
    displaydustDensity();
    balm_isr = false;
    
    //getdust();
    //getmoisture();
    data_nano.hash = calc_hash(data_nano);

    //esp_data_alarm();

    
  }

  if (haveData)
  {
    if (data_esp.hash == calc_hash(data_esp))
    {
      lcd.setCursor(19, 0);
      lcd.print(" ");

      if (timeStatus() == timeNotSet)
      {
        Rtc.SetDateTime(data_esp.timenow - 946684800);
      }
      updatereceiveddata();
    }

    printreceiveddata();

    haveData = false;
  }

  if (now() != prevDisplay)
  {
    Serial.println(now());
    prevDisplay = now();
  }
}

void updatereceiveddata()
{
  if (data_nano.temperaturein != data_esp.temperaturein || data_nano.temperatureout != data_esp.temperatureout || data_nano.humidity != data_esp.humidity)
  {
    displayTemperature();
    data_nano.temperaturein    = data_esp.temperaturein;
    data_nano.temperatureout   = data_esp.temperatureout;
    data_nano.humidity         = data_esp.humidity;
  }

  if (data_nano.powerall != data_esp.powerall || data_nano.powerac != data_esp.powerac)
  {
    displaypowerAvg(data_esp.powerall);
    data_nano.powerall         = data_esp.powerall;
    data_nano.powerac          = data_esp.powerac;
  }

  if (data_nano.nemoweight != data_esp.nemoweight)
  {
    displayNemoWeight(data_esp.nemoweight);
    data_nano.nemoweight       = data_esp.nemoweight;
  }

  if (data_nano.doorpir != data_esp.doorpir)
  {
    displayPIR(data_esp.doorpir);
    data_nano.doorpir          = data_esp.doorpir;
  }

  if (data_nano.hostall != data_esp.hostall || data_nano.hosttwo != data_esp.hosttwo)
  {
    displayHost(data_esp.hosttwo, data_esp.hostall);
    data_nano.hostall          = data_esp.hostall;
    data_nano.hosttwo          = data_esp.hosttwo;
  }

  data_nano.hash = calc_hash(data_nano);
  /*
    data_nano.accmd            = data_esp.accmd;
    data_nano.actemp           = data_esp.actemp;
    data_nano.acflow           = data_esp.acflow;
  */

}

void printreceiveddata()
{
  Serial.print("time: ");
  Serial.print(data_esp.timenow);

  Serial.print(" temperaturein: ");
  Serial.print(data_esp.temperaturein);

  Serial.print(" temperatureout: ");
  Serial.print(data_esp.temperatureout);

  Serial.print(" humidity: ");
  Serial.print(data_esp.humidity);

  Serial.print(" powerall: ");
  Serial.print(data_esp.powerall);

  Serial.print(" powerac: ");
  Serial.print(data_esp.powerac);

  Serial.print(" nemoweight: ");
  Serial.print(data_esp.nemoweight);

  Serial.print(" doorpir: ");
  Serial.print(data_esp.doorpir);

  Serial.print(" hostall: ");
  Serial.print(data_esp.hostall);

  Serial.print(" hosttwo: ");
  Serial.print(data_esp.hosttwo);

  Serial.print(" accmd: ");
  Serial.print(data_esp.accmd);

  Serial.print(" actemp: ");
  Serial.print(data_esp.actemp);

  Serial.print(" acflow: ");
  Serial.print(data_esp.acflow);

  Serial.print(" hash: ");
  if (data_esp.hash != calc_hash(data_esp)) {
    Serial.print(data_esp.hash);
    Serial.print(" != ");
    Serial.println(calc_hash(data_esp));
  }
  else
  {
    Serial.println(data_esp.hash);
  }
}

void esp_data_alarm()
{
  lcd.setCursor(19, 0);
  lcd.write(8);
  delayMicroseconds(10);

  digitalWrite(ESP_DATA_PIN, LOW);
  delayMicroseconds(100);
  digitalWrite(ESP_DATA_PIN, HIGH);
  bwrite = false;
}

void getdust()
{
  digitalWrite(DUST_OUT_PIN, LOW);
  delayMicroseconds(dust_samplingTime);
  dustMeasured = analogRead(DUST_IN_PIN);
  delayMicroseconds(dust_deltaTime);
  digitalWrite(DUST_OUT_PIN, HIGH);
  ave1.push(dustMeasured);
  dustDensity = (0.17 * (ave1.mean() * readVcc()) - 0.1);
  data_nano.nano_dustDensity = dustDensity * 100;
}

void getmoisture()
{
  moistureMeasured = analogRead(MOISTURE_PIN);
  ave2.push(moistureMeasured);
  data_nano.nano_moisture    = uint16_t(ave2.mean());
}

void displayHost(int numofhost, int numofall)
{
  lcd.setCursor(17, 2);
  if (numofall < 10)
  {
    lcd.print(' ');
  }
  lcd.print(numofall);

  lcd.setCursor(15, 2);
  lcd.print(numofhost);
}

void displayPIR(int PIR)
{
  if ( PIR == 1)
  {
    for ( int i = 0 ; i <= 3 ; i ++ )
    {
      lcd.setCursor(19, i);
      lcd.write(5);
    }
  }
  else
  {
    for ( int i = 0 ; i <= 3 ; i ++ )
    {
      lcd.setCursor(19, i);
      lcd.print(" ");
    }
  }
}

void displayNemoWeight(int nemoWeight)
{
  String str_nemoWeight = String(nemoWeight);
  int length_nemoWeight = str_nemoWeight.length();

  lcd.setCursor(2, 3);

  for ( int i = 0; i < ( 4 - length_nemoWeight ) ; i++ )
  {
    lcd.print(" ");
  }
  lcd.print(str_nemoWeight);
}

void displaypowerAvg(int Power)
{
  String str_Power = String(Power);
  int length_Power = str_Power.length();

  lcd.setCursor(10, 2);
  for ( int i = 0; i < ( 4 - length_Power ) ; i++ )
  {
    lcd.print(" ");
  }
  lcd.print(str_Power);
}

void displayTemperaturedigit(float Temperature)
{
  String str_Temperature = String(int(Temperature)) ;
  int length_Temperature = str_Temperature.length();

  for ( int i = 0; i < ( 3 - length_Temperature ) ; i++ )
  {
    lcd.print(" ");
  }
  lcd.print(Temperature, 1);
}

void displayTemperature()
{
  float tempin   = data_esp.temperaturein * 0.1;
  float tempout  = data_esp.temperatureout * 0.1;
  float humidity = data_esp.humidity * 0.1;

  lcd.setCursor(1, 1);
  displayTemperaturedigit(tempin);

  lcd.setCursor(7, 1);

  float tempdiff = tempout - tempin ;
  displayTemperaturedigit(tempout);

  lcd.setCursor(14, 1);
  if ( tempdiff > 0 )
  {
    lcd.print("+");
  }
  else if ( tempdiff < 0 )
  {
    lcd.print("-");
  }

  String str_tempdiff = String(int abs(tempdiff));
  int length_tempdiff = str_tempdiff.length();

  lcd.setCursor(15, 1);
  lcd.print(abs(tempdiff), 1);
  if ( length_tempdiff == 1)
  {
    lcd.print(" ");
  }

  lcd.setCursor(2, 2);
  if ( humidity >= 10 )
  {
    lcd.print(humidity, 1);
  }
  else
  {
    lcd.print(" ");
    lcd.print(humidity, 1);
  }
}

void digitalClockDisplay()
{
  lcd.setCursor(0, 0);
  printDigitsnocolon(month());
  lcd.print("/");
  printDigitsnocolon(day());

  lcd.setCursor(6, 0);
  lcd.print(dayShortStr(weekday()));
  lcd.setCursor(10, 0);
  printDigitsnocolon(hour());
  printDigits(minute());
  printDigits(second());
}

void printDigitsnocolon(int digits)
{
  if (digits < 10)
  {
    lcd.print('0');
  }
  lcd.print(digits);
}

void printDigits(int digits)
{
  lcd.print(":");
  if (digits < 10)
  {
    lcd.print('0');
  }
  lcd.print(digits);
}

void displaydustDensity()
{
  int n = int(dustDensity / 0.05) ;

  if ( n > 9 )
  {
    n = 9 ;
  }

  for ( int i = 0 ; i < n ; i++)
  {
    lcd.setCursor(10 + i, 3);
    lcd.write(4);
  }

  for ( int o = 0 ; o < ( 9 - n) ; o++)
  {
    lcd.setCursor(10 + n + o, 3);
    lcd.print(".");
  }
}

float readVcc()
{
  //ADMUX = _BV(MUX3) | _BV(MUX2);

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
#elif defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) || defined(__AVR_AT90USB1286__)
  ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  ADCSRB &= ~_BV(MUX5);   // Without this the function always returns -1 on the ATmega2560 http://openenergymonitor.org/emon/node/2253#comment-11432
#elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
  ADMUX = _BV(MUX5) | _BV(MUX0);
#elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
  ADMUX = _BV(MUX3) | _BV(MUX2);
#endif

  delay(2);
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  result = 1126400L / result; // Calculate Vcc (in mV);
  //result = 1074835L / result;

  // calcvoltage = ( readVcc() * 0.001 ) / 1024;
  return (float)((result * 0.001) / 1024); // Vcc in millivolts
}
