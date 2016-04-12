// esp8266
// 1) to auto reset and ground gpio0 when uploading.
// 2) to prevent DTR go to LOW while serial is opened. 

#include <avr/interrupt.h>
#include <avr/sleep.h>

int DTR_PIN   = 3;
int RESET_PIN = 0;
int FLASH_PIN = 4;

unsigned long duration;

void setup()
{
  pinMode(DTR_PIN, INPUT_PULLUP);
  pinMode(RESET_PIN, OUTPUT);
  pinMode(FLASH_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(FLASH_PIN, HIGH);
}

void loop()
{
  duration = pulseIn(DTR_PIN, LOW, 1000000);
  if (duration > 10 ) {
    // reset
    digitalWrite(FLASH_PIN, LOW);
    delay(20);
    digitalWrite(RESET_PIN, LOW);
    delay(200);
    digitalWrite(RESET_PIN, HIGH);
    delay(200);
    digitalWrite(FLASH_PIN, HIGH);
  }
}
