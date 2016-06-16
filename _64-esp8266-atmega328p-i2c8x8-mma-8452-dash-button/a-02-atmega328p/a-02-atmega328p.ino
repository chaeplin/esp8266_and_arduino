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

#define I2C_VCC 12
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
volatile bool bbutton_isr = false;

volatile struct 
{
  uint32_t hash;
  uint16_t button;
  uint16_t esp8266;
} device_data;

void button_isr() 
{
  bbutton_isr = true;
}

void turn_off_8x8() 
{
  digitalWrite(I2C_VCC, LOW);
}

void turn_on_8x8() 
{
  digitalWrite(I2C_VCC, HIGH);
}

void reset_esp() 
{
  digitalWrite(ESP_RST, LOW);
  delay(10);
  digitalWrite(ESP_RST, HIGH);
}

void requestEvent() 
{
  I2C_writeAnything(device_data);
}

void receiveEvent(int howMany) 
{
  if (howMany >= sizeof(device_data)) 
  {
    I2C_readAnything(device_data);
  }
  haveData = true;
}

void device_data_helper() 
{
  device_data.button = pir_interuptCount;
  device_data.hash = calc_hash(device_data);
}

void goingSleep() 
{
  Serial.println("going to sleep....");
  Serial.flush();
  
  attachInterrupt(digitalPinToInterrupt(BUTTON_INT), button_isr, FALLING);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  startMiils = millis();
  Serial.println("Wake up....");
}

void setup() 
{
  adc_disable();

  device_data.button = device_data.esp8266 = 0;
  device_data.hash = calc_hash(device_data);

  startMiils = millis();

  pinMode(ESP_RST, OUTPUT);
  pinMode(I2C_VCC, OUTPUT);
  pinMode(BUTTON_INT, INPUT_PULLUP);

  digitalWrite(ESP_RST, HIGH);

  Serial.begin(115200);
  while (!Serial) 
  {
    ;
  }

  Serial.println("Starting....");
  //attachInterrupt(digitalPinToInterrupt(BUTTON_INT), button_isr, FALLING);

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
}

void loop() 
{
  goingSleep();
  turn_on_8x8();
  reset_esp();

  while (1) 
  {
    if (bbutton_isr) 
    {
      Serial.println("Button pressed....");
      
      while (!digitalRead(BUTTON_INT)) 
      {
        if ((millis() - startMiils) > 500) 
        {
          break;
        }
      }

      if ((millis() - startMiils) < 500) 
      {
        pir_interuptCount = 1;
      } 
      else 
      {
        pir_interuptCount = 2;
      }

      device_data_helper();
      bbutton_isr = false;

      Serial.print("millis : ");
      Serial.println(millis() - startMiils);
      
      Serial.print("Button : ");
      Serial.println(pir_interuptCount);
    }

    if ((millis() - startMiils) > 20000) 
    {
      Serial.println("Timed out");
      break;
    }

    if (haveData) 
    {
      if (device_data.hash == calc_hash(device_data)) 
      {
        Serial.print("Msg received : ");
        Serial.println(device_data.esp8266);
        if (device_data.esp8266 == 3) 
        {
          haveData = false;
          break;
        }
      }
    }
  }

  turn_off_8x8();
  pir_interuptCount = 0;
  device_data.button = 0;
  device_data.esp8266 = 0;
  haveData = false;
  bbutton_isr = false;
  Serial.flush();
}

// -
