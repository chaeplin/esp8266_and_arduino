// base is _02-mqtt-sw-temperature
/* base config / esp8266 plus nrf24l01 */
/*
byte pipeNo;
if (radio.available(&pipeNo)) {
  uint8_t len = radio.getDynamicPayloadSize();

  if (len == sizeof(time_reqpayload)) {
    data_ackpayload.timestamp = now();
    data_ackpayload.data1 += 1;
    data_ackpayload.data2 += 5;

    radio.writeAckPayload(pipeNo, &data_ackpayload, sizeof(data_ackpayload));
    radio.read(&time_reqpayload, sizeof(time_reqpayload));
  } else {
    //
  }
}
*/
// https://github.com/PaulStoffregen/Time
#include <TimeLib.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
// https://tmrh20.github.io/RF24/
#include <nRF24L01.h>
#include <RF24.h>
// https://github.com/adafruit/TinyWireM
#include "USI_TWI_Master.h"
#include <TinyWireM.h>
// https://github.com/chaeplin/LiquidCrystal_I2C
#include <LiquidCrystal_I2C.h>

#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

// If CEN_PIN is moved to D5, D3 is available.
// define CE_PIN  5
#define CE_PIN  3
#define CSN_PIN 4

#define DEVICE_ID 25
#define CHANNEL 100

const uint64_t pipes[1] = { 0xFFFFFFFFCDLL };

struct {
  uint32_t timestamp;
  float data1;
  float data2;
} data_ackpayload;

struct {
  uint32_t timestamp;
} time_reqpayload;

RF24 radio(CE_PIN, CSN_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  time_reqpayload.timestamp = 0;
  data_ackpayload.timestamp = 0;
  data_ackpayload.data1     = 0;
  data_ackpayload.data2     = 0;

  //
  radio.begin();
  radio.setChannel(CHANNEL);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_1MBPS);
  radio.setRetries(15, 15);
  radio.setAutoAck(true);
  radio.enableAckPayload();
  radio.enableDynamicPayloads();
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();

  TinyWireM.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  setSyncProvider(getNrfTime);
  // default is 300s(5 min)
  setSyncInterval(60);
}

time_t prevDisplay = 0;

void loop() {
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();

      /*
        USICR =  (1 << USIWM0) | (1 << USICS1) | (1 << USICLK) | (1 << USITC);
        SPI.begin();
        radio.write(&time_reqpayload , sizeof(time_reqpayload));
        if (radio.isAckPayloadAvailable()) {
        uint8_t len = radio.getDynamicPayloadSize();
        if ( len == sizeof(data_ackpayload)) {
          radio.read(&data_ackpayload, sizeof(data_ackpayload));
        }
        }
      */

      USICR =  (1 << USIWM1) | (0 << USIWM0);
      digitalClockDisplay();
    }
  }
}

void digitalClockDisplay() {
  /*
    lcd.setCursor(0, 0);
    printDigitsnocolon(month());
    lcd.print("/");
    printDigitsnocolon(day());
  */
  lcd.setCursor(0, 1);
  lcd.print(data_ackpayload.data1, 0);

  lcd.setCursor(4, 1);
  lcd.print(data_ackpayload.data2, 2);

  lcd.setCursor(0, 0);
  printDigitsnocolon(hour());
  printDigits(minute());
  printDigits(second());
}

void printDigitsnocolon(int digits) {
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}

void printDigits(int digits) {
  lcd.print(":");
  if (digits < 10) {
    lcd.print('0');
  }
  lcd.print(digits);
}

time_t getNrfTime() {

  USICR =  (1 << USIWM0) | (1 << USICS1) | (1 << USICLK) | (1 << USITC);
  SPI.begin();

  uint32_t beginWait = millis();
  while (millis() - beginWait < 2500) {
    radio.write(&time_reqpayload , sizeof(time_reqpayload));
    if (radio.isAckPayloadAvailable()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len == sizeof(data_ackpayload)) {
        radio.read(&data_ackpayload, sizeof(data_ackpayload));
      }
    }

    time_reqpayload.timestamp = data_ackpayload.timestamp;
    radio.write(&time_reqpayload , sizeof(time_reqpayload));
    if (radio.isAckPayloadAvailable()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len == sizeof(data_ackpayload)) {
        radio.read(&data_ackpayload, sizeof(data_ackpayload));
      }
    }

    time_reqpayload.timestamp = data_ackpayload.timestamp;
    radio.write(&time_reqpayload , sizeof(time_reqpayload));
    if (radio.isAckPayloadAvailable()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len == sizeof(data_ackpayload)) {
        radio.read(&data_ackpayload, sizeof(data_ackpayload));
        return (unsigned long)data_ackpayload.timestamp;
      }
    }
  }
  return 0;
}

// end
