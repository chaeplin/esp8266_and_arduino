// pins
/* 24l01    85
   1  gnd   4
   2  vcc   8
   3  ce    1
   4  csn   3
   5  sck   7
   6  mosi  6
   7  miso  5
*/

#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <nRF24L01.h>
#include <RF24.h>

//Disabling ADC saves ~230uAF. Needs to be re-enable for the internal voltage check
#define adc_disable() (ADCSRA &= ~(1<<ADEN)) // disable ADC
#define adc_enable()  (ADCSRA |=  (1<<ADEN)) // re-enable ADC

#define CE_PIN 5
#define CSN_PIN 4

#define DEVICE_ID 1
#define CHANNEL 1

const uint64_t pipes[2] = { 0xFFFFFFFFFFLL, 0xCCCCCCCCCCLL };

typedef struct {
  uint32_t _salt;
  uint16_t volt;
  int16_t temp;
  int16_t humi;
  uint8_t devid;
} data;

data payload;

RF24 radio(CE_PIN, CSN_PIN);

// DS18B20
#define ONE_WIRE_BUS 3
#define TEMPERATURE_PRECISION 12

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress outsideThermometer;

void setup() {
  adc_disable();
  sensors.begin();
  if (!sensors.getAddress(outsideThermometer, 0)) {
    sleep();
  }
  sensors.setResolution(outsideThermometer, TEMPERATURE_PRECISION);
  sensors.requestTemperatures();
  payload.temp = sensors.getTempC(outsideThermometer);

  if (isnan(payload.temp) || payload.temp < -50 ) {
    sleep();
  }

  payload._salt = 0;
  payload.devid = 2;
  payload.humi  = 0;
}

void loop() {
  payload.volt = readVcc();
  sensors.requestTemperatures();
  payload.temp = sensors.getTempC(outsideThermometer) ;

  if (isnan(payload.temp) || payload.temp < -50 ) {
    sleep();
  } else {
  payload.temp = payload.temp * 10 ;

  radio.begin(); 
  radio.enableDynamicPayloads();
  radio.setAutoAck(1); 
  radio.setRetries(15, 15); 
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setPayloadSize(11);
  radio.openWritingPipe(pipes[0]);
  radio.stopListening();
  radio.write(&payload , sizeof(payload));
  delay(100);
  radio.powerDown();
  }
  sleep();
}

void sleep() {
  for (int i = 0; i < 7; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
  LowPower.powerDown(SLEEP_4S, ADC_OFF, BOD_OFF);
}

int readVcc() {
  adc_enable();
  ADMUX = _BV(MUX3) | _BV(MUX2);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high << 8) | low;

  //result = 1126400L / result; // Calculate Vcc (in mV);
  result = 1074835L / result;

  //Disable ADC
  adc_disable();

  return (int)result; // Vcc in millivolts
}
