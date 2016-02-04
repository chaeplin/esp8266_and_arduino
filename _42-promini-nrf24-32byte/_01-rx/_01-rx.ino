// 5pin
/* 24l01p    pro mini
   1  gnd   -
   2  vcc   +
   3  ce    9
   4  csn   10
   5  sck   13
   6  mosi  11
   7  miso  12
*/

#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <SPI.h>
#include "nRF24L01.h"
#include "RF24.h"
#include <printf.h>


#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CE_PIN 9
#define CSN_PIN 10

#define DEVICE_ID 10
#define CHANNEL 50

const uint64_t pipes[3] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL, 0xFFFFFFFFCCLL };

typedef struct {
  uint32_t _salt;
  uint32_t data1;
  uint32_t data2;
  uint32_t data3;
  uint32_t data4;
} data;

data sensor_data;

RF24 radio(CE_PIN, CSN_PIN);

const int doRecordPin = 3;
const int ledPin      = 13;

volatile int recordNo;
volatile boolean dorecored;

unsigned long loopcount;

void record() {
  recordNo++;
  dorecored = !dorecored;
}

void setup() {

  Serial.begin(115200);
  printf_begin();
  delay(20);
  adc_disable();
  pinMode(doRecordPin, INPUT);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW);

  loopcount = 0;
  recordNo = 0;
  dorecored = false;

  // radio
  radio.begin();
  radio.setChannel(CHANNEL);
  //radio.setPALevel(RF24_PA_LOW);
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  //radio.setAutoAck(1);
  radio.setRetries(15, 15);
  radio.enableDynamicPayloads();
  //radio.enableDynamicAck();
  radio.openReadingPipe(1, pipes[0]);
  radio.openReadingPipe(2, pipes[2]);
  //radio.openWritingPipe(pipes[1]);
  radio.startListening();

  for (int k = 0; k < 10; k = k + 1) {
    if (k % 2 == 0) {
      digitalWrite(ledPin, HIGH);
    }
    else {
      digitalWrite(ledPin, LOW);
    }
    delay(100);
  }
  radio.printDetails();
}

void loop() {

  if (radio.available()) {
    while (radio.available()) {
      uint8_t len = radio.getDynamicPayloadSize();
      if ( len != sizeof(sensor_data) ) {
        return;
      }

      radio.read(&sensor_data, sizeof(sensor_data));
      
      Serial.print(" ****** radio ======> size : ");
      Serial.print(len);
      Serial.print(" _salt : ");
      Serial.print(sensor_data._salt);
      Serial.print(" data1 : ");
      Serial.print(sensor_data.data1);
      Serial.print(" data2 : ");
      Serial.print(sensor_data.data2);
      Serial.print(" data3 : ");
      Serial.print(sensor_data.data3);
      Serial.print(" data4 :");
      Serial.println(sensor_data.data4);

    }
  }
}
//radio.read(&sensor_data, sizeof(sensor_data));
