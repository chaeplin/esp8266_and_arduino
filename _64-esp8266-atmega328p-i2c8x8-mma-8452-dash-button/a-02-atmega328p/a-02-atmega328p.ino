// Clock internal 8MHz / Board : Atmega328 on breadboard
#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <Wire.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define I2C_SLAVE_ADDR  8
#define I2C_MATRIX_ADDR 0x70

#define HT16K33_SS            B00100000 // System setup register
#define HT16K33_SS_STANDBY    B00000000 // System setup - oscillator in standby mode
#define HT16K33_SS_NORMAL     B00000001 // System setup - oscillator in normal mode

#define ESP_RST 17
#define BUTTON_INT 2 // int 0
// not used yet
//#define MMA_INT 3

uint8_t HT16K33_i2c_write(uint8_t val) {
  Wire.beginTransmission(I2C_MATRIX_ADDR);
  Wire.write(val);
  return Wire.endTransmission();
} // _i2c_write

// Put the chip to sleep
//
uint8_t HT16K33_sleep() {
  return HT16K33_i2c_write(HT16K33_SS | HT16K33_SS_STANDBY); // Stop oscillator
} // sleep

/****************************************************************/
// Wake up the chip (after it been a sleep )
//
uint8_t HT16K33_normal() {
  return HT16K33_i2c_write(HT16K33_SS | HT16K33_SS_NORMAL); // Start oscillator
} // normal

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

unsigned long startMiils;
uint16_t pir_interuptCount = 0;
volatile bool haveData = false;
volatile bool bbutton_downp_isr = false;
bool dataUpdated = false;

unsigned long duration;

typedef struct
{
  uint32_t hash;
  uint16_t button;
  uint16_t esp8266;
} data;

volatile data device_esp;
volatile data device_pro;

//http://www.pial.net/8x8-dot-matrix-font-generator-based-on-javascript-and-html/
static const uint8_t PROGMEM
smile_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10100101,
  B10011001,
  B01000010,
  B00111100
},
/*
light[] =
{ B00011000,
  B01100110,
  B01000010,
  B10011001,
  B10011001,
  B01000010,
  B01100110,
  B00011000
},
ac[] =
{ B10011001,
  B01000010,
  B00100100,
  B10011001,
  B10011001,
  B00100100,
  B01000010,
  B10011001
},
*/
light[] =
{ B00111100,
  B01000010,
  B10000101,
  B10001001,
  B10010001,
  B01100110,
  B00011000,
  B00011000
},
ac[] =
{ B01111111,
  B11000000,
  B10111110,
  B10100000,
  B10100111,
  B10110100,
  B11000000,
  B01111111
},
off[] =
{ B00011000,
  B11011011,
  B10011001,
  B10011001,
  B10000001,
  B10000001,
  B10000001,
  B01111110
};

Adafruit_8x8matrix matrix = Adafruit_8x8matrix();

void button_down_isr()
{
  bbutton_downp_isr = true;
  startMiils = millis();
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

  HT16K33_sleep();
  delay(10);

  pir_interuptCount = 0;
  bbutton_downp_isr = haveData = dataUpdated = false;
  device_pro.button = device_pro.esp8266 = 0;
  device_pro.hash = calc_hash(device_pro);

  attachInterrupt(digitalPinToInterrupt(BUTTON_INT), button_down_isr, FALLING);
  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  HT16K33_normal();

  matrix.begin(I2C_MATRIX_ADDR);
  matrix.setBrightness(15);
  matrix.setRotation(3);

  matrix.clear();
  matrix.drawRect(0, 0, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(200);

  matrix.fillRect(2, 2, 4, 4, LED_ON);
  matrix.writeDisplay();
  delay(200);
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

  Serial.println("\r\nStarting....");

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  goingSleep();
}

void loop()
{
  if (bbutton_downp_isr && !dataUpdated)
  {
    pir_interuptCount++;
    if (pir_interuptCount > 3)
    {
      pir_interuptCount = 1;
    }

    Serial.flush();
    Serial.print("Wake up with button: ");
    Serial.println(pir_interuptCount);

    matrix.clear();
    switch (pir_interuptCount)
    {
      case 1:
        matrix.drawBitmap(0, 0, light, 8, 8, LED_ON);
        break;

      case 2:
        matrix.drawBitmap(0, 0, ac, 8, 8, LED_ON);
        break;

      case 3:
        matrix.drawBitmap(0, 0, off, 8, 8, LED_ON);
        break;

      default:
        matrix.drawBitmap(0, 0, smile_bmp, 8, 8, LED_ON);
        break;
    }
    matrix.writeDisplay();
    bbutton_downp_isr = false;
  }

  if (((millis() - startMiils) > 2000) && !dataUpdated)
  {
    device_pro.button = pir_interuptCount;
    device_pro.hash = calc_hash(device_pro);

    Serial.print("device_pro.button: ");
    Serial.println(device_pro.button);
    reset_esp();
    dataUpdated = true;
  }

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
