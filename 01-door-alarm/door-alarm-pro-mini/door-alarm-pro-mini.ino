
/*

2 - int 0 - door

7 - esp 5
8 - esp 16
9 - esp rest

5 - buzzer vcc
6 - buzzer i/o

*/

#include <avr/interrupt.h>
#include <avr/sleep.h>

int wakeUpPin     = 2;

int espDoorPin    = 7;
int espRfStatePin = 8;   
int espResetPin   = 9;

int buzzerVccPin  = 5;
int buzzerIoPin   = 6;

long startMills;

int espRfstate = HIGH;

int alarmset = LOW ;

volatile int doorStatus ;

void setup()
{
  Serial.begin(38400);
  Serial.println("Starting sleep test");
  delay(100);  
  pinMode(wakeUpPin, INPUT_PULLUP);

  doorStatus = digitalRead(wakeUpPin);
  
  pinMode(espRfStatePin, INPUT); 
  pinMode(espResetPin, OUTPUT);
  pinMode(espDoorPin, OUTPUT);

  pinMode(buzzerVccPin, OUTPUT);
  pinMode(buzzerIoPin, OUTPUT);
  
  attachInterrupt(0, WakeUp, CHANGE);
  
  startMills = millis();
  
  digitalWrite(espResetPin, HIGH);
  digitalWrite(espDoorPin, doorStatus);

  digitalWrite(buzzerVccPin, LOW);
  digitalWrite(buzzerIoPin, HIGH);  

  delay(250);
  sleepNow();

}

void sleepNow()
{
  Serial.println("Going sleep");
  delay(100);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  attachInterrupt(0, WakeUp, CHANGE);
  
  sleep_mode();   
  sleep_disable();
  Serial.println("Wake up at sleepNow");
  
}

void WakeUp()   
{ 
  if ((millis() - startMills) > 300) {  
    Serial.print("======> Wake up :  ");
    Serial.println(millis() - startMills);
    //doorStatus = ! doorStatus ;
    doorStatus = digitalRead(wakeUpPin);
    alarmset = HIGH ;
    digitalWrite(espDoorPin, doorStatus); 
    startMills = millis();
    espReset();
  }
}

void loop()
{
   
  espRfstate = digitalRead(espRfStatePin);
  
  if (((millis() - startMills) >= 5000) && (espRfstate == HIGH))
  {
      Serial.println(millis() - startMills);
      alarm_wifi();
      sleepNow();
  }

  if (espRfstate == LOW) 
  {
       Serial.println(millis() - startMills);
       Serial.println("msg sent");
       sleepNow();
  }

  if ( alarmset == HIGH ) {
     Serial.println(millis() - startMills);
     if ( doorStatus == HIGH ) 
     {
       alarm_open();
     } else {
       alarm_close();
     }
  }
     
  delay(10);
}


void espReset()
{
  Serial.println("Reset ESP");
  digitalWrite(espResetPin, LOW);
  delay(10);
  digitalWrite(espResetPin, HIGH);
}


void alarm_wifi()
{
  Serial.println("===> wifi has problem");
  
  digitalWrite(buzzerVccPin, HIGH);
  
  digitalWrite(buzzerIoPin, LOW);
  delay(300);
  digitalWrite(buzzerIoPin, HIGH);  
  delay(100);

  digitalWrite(buzzerIoPin, LOW);
  delay(300);
  digitalWrite(buzzerIoPin, HIGH);

  digitalWrite(buzzerVccPin, LOW);
  
  alarmset = LOW ;
  
}

void alarm_open()
{
  Serial.println("===> door opened");
  
  digitalWrite(buzzerVccPin, HIGH);
  
  digitalWrite(buzzerIoPin, LOW);
  delay(100);
  digitalWrite(buzzerIoPin, HIGH);
  
  digitalWrite(buzzerVccPin, LOW);  
  
  alarmset = LOW ;
}

void alarm_close()
{
  Serial.println("===> door closed");
  
  digitalWrite(buzzerVccPin, HIGH);
  
  digitalWrite(buzzerIoPin, LOW);
  delay(80);
  digitalWrite(buzzerIoPin, HIGH);  
  delay(100);
  digitalWrite(buzzerIoPin, LOW);
  delay(80);
  digitalWrite(buzzerIoPin, HIGH);

  digitalWrite(buzzerVccPin, LOW);  
  
  alarmset = LOW ;
}

