#include <Average.h>

#define Trig_pin 11

volatile unsigned long echoBuffer[2];

volatile unsigned int x = 0;

Average<float> ave(5);

void setup() {
  Serial.begin (38400);
  pinMode(Trig_pin, OUTPUT);
  pinMode(2, INPUT_PULLUP);
}

long check_distance() {

  x = 0 ;

  int distance = 0;

  for ( int i = 0 ; i < 2 ; i++ ) {
    echoBuffer[i] = 0;
  }

  attachInterrupt(0, echo_Interrupt_Handler, CHANGE);

  delay(200);

  digitalWrite(Trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trig_pin, LOW);

  delay(500);

  if ( echoBuffer[1] > echoBuffer[0]   ) {
    distance = ( echoBuffer[1] - echoBuffer[0] ) / 29 / 2 ;
  } else {
    distance = 1000 ;
  }

  detachInterrupt(0);

  return distance;
  
}

void loop() {

  int distance = check_distance();
  ave.push(distance);

  Serial.print("distance : ");
  Serial.print(distance);
  Serial.print(" ave.mean : ");
  Serial.print(int(ave.mean()));
  Serial.print(" ave.stddev : ");
  Serial.println(int(ave.stddev()));

  delay(1000); // pause 5 secs
}


void echo_Interrupt_Handler() {
  echoBuffer[x++] = micros();
}
