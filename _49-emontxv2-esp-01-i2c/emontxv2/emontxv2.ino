#include <avr/wdt.h>
#include <Wire.h>
// https://github.com/Makuna/Rtc
#include <RtcDS1307.h>
#include "PinChangeInterrupt.h"
#include "EmonLib.h"
/*
  atmega328  -  esp - ds1307
  3v
  gnd
  scl - d2  - scl
  sda - d0  - sda // led
  d2 -      - sqw
  d6  - tx
  d7  - rst
  d12 -  -        // button to inform flashing of esp
*/

// pins
// IN
#define SQW_PIN         2   // int 0
#define BOOT_MODE_PIN   12  // put esp in booot mode
#define PLS_CNT_PIN     3   // int 1, pulse count
#define DOOR_LOCK_PIN   4   // Onewire ds18b20 is not used  

// OUT
#define LED_PIN         9
#define CHECK_TWI_PIN   6
#define RESET_ESP_PIN   7   // reset esp

// retain pulse data
#define RETAIN_PULSE true

// rtc size --> 56 byte
static const uint8_t RTC_WRITE_ADDRESS = 0;

const int CT1 = 1;
const int CT2 = 1;
const int CT3 = 1;

volatile bool bsqw_pulse;
volatile bool bmode_start;
volatile bool breset_esp01;
volatile bool bboot_done;
volatile bool btwi_idle;
volatile bool bsensor_write;
volatile bool bdoor_now;
volatile bool bdoor_write;

volatile uint16_t pulseValue = 0;
volatile uint32_t pulseCount = 0;
volatile uint32_t pulseTime;
volatile uint32_t lastTime;

//
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length) {
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data) {
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

struct {
  uint32_t hash;
  uint32_t pls_no;
  uint16_t pls_ts;
  int16_t ct1_rp;
  int16_t ct1_ap;
  int16_t ct1_vr;
  uint16_t ct1_ir;
  int16_t ct2_rp;
  int16_t ct3_rp;
  uint16_t door;
} sensor_data;

//
EnergyMonitor ct1, ct2, ct3;
RtcDS1307 Rtc;

void onpulse_isr() {
  lastTime = pulseTime;
  pulseTime = millis();

  if (( millis() - lastTime ) < 600 ) {
    return;
  }
  pulseValue =  (pulseTime - lastTime);
  pulseCount++;
}

void start_onpulse_isr() {
  detachInterrupt(digitalPinToInterrupt(PLS_CNT_PIN));
  pulseTime = millis();
  attachInterrupt(digitalPinToInterrupt(PLS_CNT_PIN), onpulse_isr, FALLING);
}

void door_lock_isr() {
  bdoor_now = !bdoor_now;
  bdoor_write = true;
}

void twi_busy_isr() {
  btwi_idle = digitalRead(CHECK_TWI_PIN);
}

void sqw_isr() {
  digitalWrite(LED_PIN, HIGH);
  bsqw_pulse = true;
}

void boot_mode_isr() {
  bmode_start = !bmode_start;
  breset_esp01 = true;
}

void _reset_esp01(bool upload) {
  bboot_done = false;
  Wire.end();
  digitalWrite(RESET_ESP_PIN, LOW);
  if (upload == true) {
    pinMode(CHECK_TWI_PIN, INPUT_PULLUP);
    pinMode(SDA, OUTPUT);
    pinMode(SCL, INPUT_PULLUP);

    digitalWrite(SDA, LOW);
  } else {
    pinMode(SCL, INPUT_PULLUP);
    pinMode(SDA, INPUT_PULLUP);
  }

  delay(3);
  digitalWrite(RESET_ESP_PIN, HIGH);
  delay(200);

  if (upload == false) {
    bboot_done = true;
    Wire.begin();
  } else {
    //
  }
}

void start_measure() {
  ct1.calcVI(20, 2000);
  ct2.calcVI(20, 2000);
  ct3.calcVI(20, 2000);

  sensor_data.ct1_rp = ct1.realPower;
  sensor_data.ct1_ap = ct1.apparentPower;
  sensor_data.ct1_vr = ct1.Vrms*100;
  sensor_data.ct1_ir = ct1.Irms*100;
  sensor_data.ct2_rp = ct2.realPower;
  sensor_data.ct3_rp = ct3.realPower;
  sensor_data.pls_no = pulseCount;
  sensor_data.pls_ts = pulseValue;
  sensor_data.door   = bdoor_now;
  sensor_data.hash   = calc_hash(sensor_data);

  digitalWrite(LED_PIN, LOW);
  Serial.print(sensor_data.ct1_rp); Serial.print(" "); Serial.println(ct1.realPower);

  bsensor_write = true;
}

bool write_nvram() {
  if ( btwi_idle ) {
    pinMode(CHECK_TWI_PIN, OUTPUT);
    digitalWrite(CHECK_TWI_PIN, LOW);
    delayMicroseconds(5);

    const uint8_t* to_write_current = reinterpret_cast<const uint8_t*>(&sensor_data);
    uint8_t gotten = Rtc.SetMemory(RTC_WRITE_ADDRESS, to_write_current, sizeof(sensor_data));

    delayMicroseconds(5);
    digitalWrite(CHECK_TWI_PIN, HIGH);
    pinMode(CHECK_TWI_PIN, INPUT_PULLUP);
    return true;
  } else {
    return false;
  }
}

bool read_nvram() {
  if (digitalRead(CHECK_TWI_PIN)) {
    pinMode(CHECK_TWI_PIN, OUTPUT);
    digitalWrite(CHECK_TWI_PIN, LOW);
    delayMicroseconds(5);

    uint8_t* to_read_current = reinterpret_cast< uint8_t*>(&sensor_data);
    uint8_t gotten = Rtc.GetMemory(RTC_WRITE_ADDRESS, to_read_current, sizeof(sensor_data));

    delayMicroseconds(5);
    digitalWrite(CHECK_TWI_PIN, HIGH);
    pinMode(CHECK_TWI_PIN, INPUT_PULLUP);
    return true;
  } else {
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  wdt_enable(WDTO_8S);

  // bool
  bsqw_pulse = bmode_start = breset_esp01 = bboot_done = bsensor_write = bdoor_write = false;
  btwi_idle = true;

  // pin out
  pinMode(LED_PIN, OUTPUT);
  pinMode(RESET_ESP_PIN, OUTPUT);

  // pin in
  pinMode(SQW_PIN, INPUT_PULLUP);
  pinMode(BOOT_MODE_PIN, INPUT_PULLUP);
  pinMode(CHECK_TWI_PIN, INPUT_PULLUP);
  pinMode(PLS_CNT_PIN, INPUT_PULLUP);
  pinMode(DOOR_LOCK_PIN, INPUT_PULLUP);

  // pin out
  digitalWrite(RESET_ESP_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);

  // reset esp01
  _reset_esp01(false);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  ct1.voltageTX(212.26, 1.7);
  ct1.currentTX(1, 111.1);

  ct2.voltageTX(212.26, 1.7);
  ct2.currentTX(2, 111.1);

  ct3.voltageTX(212.26, 1.7);
  ct3.currentTX(3, 111.1);

  while (!digitalRead(CHECK_TWI_PIN)) {
    delay(10);
  }
  bool ok = read_nvram();
  if (!ok || sensor_data.hash != calc_hash(sensor_data) || !RETAIN_PULSE ) {
    sensor_data.pls_no = 0;
  } else if (RETAIN_PULSE) {
    pulseCount = sensor_data.pls_no;
  }

  //
  sensor_data.door = bdoor_now = digitalRead(DOOR_LOCK_PIN);
  sensor_data.hash = calc_hash(sensor_data);

  // rtc
  Rtc.Begin();
  Rtc.SetIsRunning(true);
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_1Hz);

  // interrupt
  attachInterrupt(digitalPinToInterrupt(SQW_PIN), sqw_isr, FALLING);               //  int 0
  attachInterrupt(digitalPinToInterrupt(PLS_CNT_PIN), start_onpulse_isr, FALLING); //  int 1
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(BOOT_MODE_PIN), boot_mode_isr, FALLING);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CHECK_TWI_PIN), twi_busy_isr, CHANGE);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(DOOR_LOCK_PIN), door_lock_isr, CHANGE);
}

void loop() {
  wdt_reset();
  if (bsqw_pulse) {
    start_measure();
    bsqw_pulse = false;
  }

  if (!breset_esp01 && bboot_done) {
    if (bsensor_write || bdoor_write) {
      if (btwi_idle) {
        if (write_nvram()) {
          if (bsensor_write)
            bsensor_write = false;
          if (bdoor_write)
            bdoor_write = false;
        }
      }
    }
  }

  if (breset_esp01) {
    _reset_esp01(bmode_start);
    breset_esp01 = false;
  }
}
