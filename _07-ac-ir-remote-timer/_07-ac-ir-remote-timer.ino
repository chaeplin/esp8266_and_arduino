#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <IRremote.h>

const int wakeUpPin = 2;
IRsend irsend;

volatile int s = LOW;
int o_s = LOW;
int swstatus;


void setup() {
  Serial.begin(38400);
  pinMode(wakeUpPin, INPUT_PULLUP);
  attachInterrupt(0, WakeUp, CHANGE);
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
  Serial.print("======> Wake up :  ");
  s= !s;
}

void send_ir(){
  if ( swstatus == LOW ) {
    Serial.println("======> mode on ");
    irsend.sendNEC(0xFF02FD, 32); 
    delay(10);
  } else {
    Serial.println("======> mode off ");
    irsend.sendNEC(0xFF9867, 32);
    delay(10);
  }
}

void loop()
{
  if ( s != o_s ) {
    swstatus = digitalRead(wakeUpPin);
    Serial.println(swstatus);
    send_ir();
    o_s = s;
  }
  sleepNow();
}

