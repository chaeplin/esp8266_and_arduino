#include <avr/pgmspace.h>
#include <Wire.h>
#include <RtcDS1307.h>

#define SquareWave_Pin 2 // int 0
#define LED_PIN 9

RtcDS1307 Rtc;

void SquareWaveCheck() {
  digitalWrite(LED_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  Rtc.Begin();
  Rtc.SetIsRunning(true);
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_1Hz);

  pinMode(LED_PIN, OUTPUT);
  pinMode(SquareWave_Pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SquareWave_Pin), SquareWaveCheck, FALLING);
  Wire.end();
  
  pinMode(SDA, OUTPUT);
}

void loop() {
  digitalWrite(SDA, LOW);
  delay(200);
  digitalWrite(LED_PIN, LOW);

}
