#include <LowPower.h>
// https://github.com/rocketscream/Low-Power

int powerPin = 2;

void setup() {
  delay(500);
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);
  delay(500);
}

void loop()
{
    delay(500);
    togglePower();
    delay(500);
    sleep30Minutes();
    delay(500);
    togglePower();
    delay(500);
    sleep30Minutes();
    delay(500);
}

// USe 9 sec
void sleep60Minutes()
{
  for (int i = 0; i < 400; i++) { 
     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
  }
}


void sleep30Minutes()
{
  for (int i = 0; i < 200; i++) { 
     LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
  }
}

void togglePower() {
  delay(100);              
  digitalWrite(powerPin, HIGH);  
  delay(100);              
  digitalWrite(powerPin, LOW);    
  delay(100);              
}