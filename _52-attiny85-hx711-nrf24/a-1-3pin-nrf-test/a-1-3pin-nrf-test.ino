#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <nRF24L01.h>
#include <RF24.h>

#define CE_PIN  3
#define CSN_PIN 3

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
  
  
}

void loop() {
  getnrfdata();
  delay(5000);
}


void getnrfdata() {
  time_reqpayload.timestamp = data_ackpayload.timestamp ;
  radio.write(&time_reqpayload , sizeof(time_reqpayload));
  if (radio.isAckPayloadAvailable()) {
    uint8_t len = radio.getDynamicPayloadSize();
    if ( len == sizeof(data_ackpayload)) {
      radio.read(&data_ackpayload, sizeof(data_ackpayload));
    }
  }
}
