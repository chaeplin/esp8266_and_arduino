// Using low poer for attiny85 
// https://github.com/johnnie502/Low-Power
#include <LowPower.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

void setup() {
  pinMode(3, OUTPUT);
  for (int k = 0; k < 10; k = k + 1) {
    if (k % 2 == 0) {
      digitalWrite(3, HIGH);
    }
    else {
      digitalWrite(3, LOW);
    }
    delay(250);
  }
}

void loop() {
  sleep16seconds();
  digitalWrite(3, HIGH);  
  delay(1000);             
  digitalWrite(3, LOW);    
}


void sleep16seconds() {
  for (int i = 0; i < 2; i++) { 
     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
  }  
}
