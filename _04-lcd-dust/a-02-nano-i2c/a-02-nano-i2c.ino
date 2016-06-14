#include <avr/pgmspace.h>
#include <IRremote.h>
#include <Wire.h>
#include <Average.h>
#include <avr/pgmspace.h>
#include <TimeLib.h>

/* pins
  D9 : ir in
  D8 : dust ir out
  D3 : ir out
  D2 : sqwe in
  A0 : dust in
  A1 : moisture in
*/

#define IR_IN_PIN 9
#define DUST_OUT_PIN 8
#define IR_OUT_PIN 3
#define SQWV_PIN 2

#define DUST_IN_PIN A0
#define MOISTURE_PIN A1

#define dust_samplingTime 280
#define dust_deltaTime 40

volatile struct
{
  uint32_t hash;
  float dustDensity;
  uint16_t moisture;
  uint16_t irrecvd;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t acauto;
} data_nano;

volatile struct
{
  uint32_t hash;
  float tempeinside;
  float tempeoutside;
  uint8_t accmd;
  uint8_t actemp;
  uint8_t acflow;
  uint8_t acauto;
} data_esp;

int dustMeasured = 0;
int moistureMeasured = 0;
float dustDensity = 0;
float moisture = 0;

volatile bool haveData = false;
volatile bool balm_isr;

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

template <typename T> unsigned int I2C_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = Wire.read();
  return i;
}

template <typename T> unsigned int I2C_writeAnything (const T& value)
{
  Wire.write((byte *) &value, sizeof (value));
  return sizeof (value);
}

void alm_isr()
{
  balm_isr = true;
}

void setup()
{
  Serial.begin(115200);

  data_nano.dustDensity  = 0;
  data_nano.moisture     = 0;
  data_nano.irrecvd      = 0;
  data_nano.accmd        = 0;
  data_nano.actemp       = 27;
  data_nano.acflow       = 1;
  data_nano.acauto       = 0;
  data_nano.hash         = calc_hash(data_nano);

  balm_isr = false;

  pinMode(SQWV_PIN, INPUT_PULLUP);
  pinMode(DUST_OUT_PIN, OUTPUT);

  digitalWrite(DUST_OUT_PIN, HIGH);

  Wire.begin(8);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  attachInterrupt(0, alm_isr, FALLING);
}

void loop()
{
  if (balm_isr)
  {
    getdust();
    getmoisture();
    balm_isr = false;
  }

  if (haveData)
  {
    data_nano.dustDensity = dustDensity;
    data_nano.moisture    = moisture;
    data_nano.hash        = calc_hash(data_nano);
    haveData = false;
  }
}

void requestEvent()
{
  I2C_writeAnything(data_nano);
}

void receiveEvent(int howMany)
{
  if (howMany >= sizeof(data_esp))
  {
    I2C_readAnything(data_esp);
  }

  if (data_esp.hash == calc_hash(data_esp))
  {
    haveData = true;
  }
}

void getdust()
{
  digitalWrite(DUST_OUT_PIN, LOW);
  delayMicroseconds(dust_samplingTime);
  ave1.push(analogRead(DUST_IN_PIN));
  //dustMeasured = analogRead(DUST_IN_PIN);
  delayMicroseconds(dust_deltaTime);
  digitalWrite(DUST_OUT_PIN, HIGH);
  //ave1.push(dustMeasured);
  dustDensity = (0.17 * (ave1.mean() * readVcc()) - 0.1);
}

void getmoisture()
{
  ave2.push(analogRead(MOISTURE_PIN));
  //moistureMeasured = analogRead(MOISTURE_PIN);
  //ave2.push(moistureMeasured);
  moisture = ave2.mean();
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
