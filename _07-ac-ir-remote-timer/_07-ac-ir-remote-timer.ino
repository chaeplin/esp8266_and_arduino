#include <LowPower.h>
// https://github.com/rocketscream/Low-Power

int powerPin = 2;

void setup() {
  
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);    
}

void loop()
{
    togglePower();
    sleep30Minutes();
    togglePower();
    //sleep60Minutes();
    sleep30Minutes();
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
  digitalWrite(powerPin, HIGH);  
  delay(100);              
  digitalWrite(powerPin, LOW);    
  delay(100);              
}
