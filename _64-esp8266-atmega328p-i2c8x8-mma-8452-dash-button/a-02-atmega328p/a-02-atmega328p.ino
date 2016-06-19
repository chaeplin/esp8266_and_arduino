// Clock internal 8MHz / Board : Atmega328 on breadboard
#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <Wire.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define I2C_SLAVE_ADDR  8

#define ESP_RST 17
#define BUTTON_INT 2 // int 0
// not used yet
//#define MMA_INT 3

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

/* for i2c */
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

unsigned int startMiils;
uint16_t pir_interuptCount = 0;
volatile bool haveData = false;
volatile bool bbutton_up_isr = false;

unsigned long duration;

typedef struct
{
  uint32_t hash;
  uint16_t button;
  uint16_t esp8266;
} data;

volatile data device_esp;
volatile data device_pro;

void button_up_isr()
{
  detachInterrupt(digitalPinToInterrupt(BUTTON_INT));
  duration = millis() - startMiils;
  bbutton_up_isr = true;
}

void button_down_isr()
{
  startMiils = millis();
  detachInterrupt(digitalPinToInterrupt(BUTTON_INT));
  attachInterrupt(digitalPinToInterrupt(BUTTON_INT), button_up_isr, RISING);
}

void reset_esp()
{
  Serial.println("Reseting esp....");
  digitalWrite(ESP_RST, LOW);
  delay(5);
  digitalWrite(ESP_RST, HIGH);
}

void requestEvent()
{
  I2C_writeAnything(device_pro);
}

void receiveEvent(int howMany)
{
  if (howMany >= sizeof(device_esp))
  {
    I2C_readAnything(device_esp);
  }
  if (device_esp.hash == calc_hash(device_esp))
  {
    haveData = true;
  }
}

void goingSleep()
{
  Serial.println("going to sleep....");
  Serial.flush();

  bbutton_up_isr = false;
  haveData = false;
  device_pro.button = device_pro.esp8266 = 0;
  device_pro.hash = calc_hash(device_pro);

  attachInterrupt(digitalPinToInterrupt(BUTTON_INT), button_down_isr, FALLING);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  Serial.println("Wake up....");
  reset_esp();
}

void setup()
{
  adc_disable();

  device_pro.button = device_pro.esp8266 = 0;
  device_pro.hash = calc_hash(device_pro);

  pinMode(ESP_RST, OUTPUT);
  pinMode(BUTTON_INT, INPUT_PULLUP);

  digitalWrite(ESP_RST, HIGH);

  Serial.begin(115200);
  Serial.flush();

  Serial.println("Starting....");

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  goingSleep();
}

void loop()
{
  while (!bbutton_up_isr)
  {
    if ((millis() - startMiils) > 500)
    {
      detachInterrupt(digitalPinToInterrupt(BUTTON_INT));
      duration = 500;
      bbutton_up_isr = true;
      break;
    }
  }

  if (duration < 500)
  {
    pir_interuptCount = 1;
  }
  else
  {
    pir_interuptCount = 2;
  }

  device_pro.button = pir_interuptCount;
  device_pro.hash = calc_hash(device_pro);
  
  if (haveData)
  {
    Serial.println("Got msg");
    goingSleep();
  }

  if ((millis() - startMiils) > 20000)
  {
    Serial.println("Timed out");
    goingSleep();
  }
}

// -
