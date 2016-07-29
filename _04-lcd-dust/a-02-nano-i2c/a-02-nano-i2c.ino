#include <avr/pgmspace.h>
#include <IRremote.h>
#include <lgWhisen.h>
#include <Wire.h>
#include <Average.h>
#include <avr/pgmspace.h>

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
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
  uint8_t ac_etc;
} current_nano;

volatile struct
{
  uint32_t hash;
  uint16_t dustDensity;
  uint16_t moisture;
  uint8_t ir_recvd;
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
} data_nano;

volatile struct
{
  uint32_t hash;
  uint8_t ac_mode;
  uint8_t ac_temp;
  uint8_t ac_flow;
  uint8_t ac_etc;
} data_esp;

int dustMeasured = 0;
int moistureMeasured = 0;
float dustDensity = 0;
float moisture = 0;

volatile bool haveData = false;
volatile bool balm_isr;

Average<float> ave1(10);
Average<float> ave2(10);

IRrecv irrecv(IR_IN_PIN);
lgWhisen lgWhisen(0, 0);

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
  Serial.println("Starting....");

  current_nano.ac_mode = 0;
  current_nano.ac_temp = 27;
  current_nano.ac_flow = 0;
  current_nano.ac_etc  = 0;

  data_nano.dustDensity  = 0;
  data_nano.moisture     = 0;
  data_nano.ir_recvd     = 0;
  data_nano.ac_mode      = current_nano.ac_mode;
  data_nano.ac_temp      = current_nano.ac_temp;
  data_nano.ac_flow      = current_nano.ac_flow;
  data_nano.hash         = calc_hash(data_nano);

  balm_isr = false;

  lgWhisen.setTemp(current_nano.ac_temp);
  lgWhisen.setFlow(current_nano.ac_flow);
  irrecv.enableIRIn();

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
  decode_results results;

  if (irrecv.decode(&results))
  {
    if (lgWhisen.decode(&results))
    {
      Serial.println("IR Received....");
      data_nano.ir_recvd = 1;

      if (lgWhisen.get_ir_mode() != 0)
      {
        data_nano.ac_mode = 1;
      }
      else
      {
        data_nano.ac_mode = 0;
      }

      data_nano.ac_temp = lgWhisen.get_ir_temperature();
      data_nano.ac_flow = lgWhisen.get_ir_flow();
    }
    irrecv.enableIRIn();
  }

  if (balm_isr)
  {
    getdust();
    getmoisture();

    data_nano.dustDensity  = dustDensity * 1000;
    data_nano.moisture     = moisture;
    if (data_nano.ir_recvd == 0)
    {
      data_nano.ac_mode = current_nano.ac_mode;
      data_nano.ac_temp = current_nano.ac_temp;
      data_nano.ac_flow = current_nano.ac_flow;
    }
    balm_isr = false;
  }

  if (haveData && data_nano.ir_recvd == 0)
  {
    Serial.println("I have data...");

    current_nano.ac_temp = data_esp.ac_temp;
    current_nano.ac_flow = data_esp.ac_flow;

    lgWhisen.setTemp(current_nano.ac_temp);
    lgWhisen.setFlow(current_nano.ac_flow);

    switch (data_esp.ac_mode)
    {
      // ac power down
      case 0:
        Serial.println("IR -----> AC Power Down");
        lgWhisen.power_down();
        //delay(5);
        irrecv.enableIRIn();
        break;

      // ac on
      case 1:
        Serial.println("IR -----> AC Power On");
        lgWhisen.activate();
        //delay(2);
        //lgWhisen.airclean_on();
        //delay(5);
        irrecv.enableIRIn();
        break;

      default:
        break;
    }
    current_nano.ac_mode = data_esp.ac_mode;
    haveData = false;
  }
}

void requestEvent()
{
  data_nano.hash = calc_hash(data_nano);
  I2C_writeAnything(data_nano);
  if (data_nano.ir_recvd == 1)
  {
    data_nano.ir_recvd = 0;
  }
}

void receiveEvent(int howMany)
{
  if (howMany >= sizeof(data_esp))
  {
    I2C_readAnything(data_esp);
    if (data_esp.hash == calc_hash(data_esp))
    {
      haveData = true;
    }
  }
}

void getdust()
{
  digitalWrite(DUST_OUT_PIN, LOW);
  delayMicroseconds(dust_samplingTime);
  ave1.push(analogRead(DUST_IN_PIN));
  delayMicroseconds(dust_deltaTime);
  digitalWrite(DUST_OUT_PIN, HIGH);
  dustDensity = (0.17 * (ave1.mean() * readVcc()) - 0.1);
}

void getmoisture()
{
  ave2.push(analogRead(MOISTURE_PIN));
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
