#include <Average.h>

#define Trig_pin 4

volatile unsigned long echoBufferx[2];
volatile unsigned long echoBuffery[2];

volatile unsigned int x = 0;
volatile unsigned int y = 0;

Average<float> ave(5);

void setup() {
  Serial.begin (38400);
  pinMode(Trig_pin, OUTPUT);
  pinMode(2, INPUT_PULLUP);
  pinMode(3, INPUT_PULLUP);
}

long check_distance() {
  
  x = 0 ;
  y = 0 ;

  int distancex = 0;
  int distancey = 0;

  for ( int i = 0 ; i < 2 ; i++ ) {
    echoBufferx[i] = 0;
    echoBuffery[i] = 0;
  }

  attachInterrupt(0, echo1_Interrupt_Handler, CHANGE);
  attachInterrupt(1, echo2_Interrupt_Handler, CHANGE);

  delay(200);

  digitalWrite(Trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(Trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(Trig_pin, LOW);

  delay(300);

  if ( echoBufferx[1] > echoBufferx[0]   ) {
    distancex = ( echoBufferx[1] - echoBufferx[0] ) / 29 / 2 ;
  } else {
    distancex = 1000 ;
  }

  if ( echoBuffery[1] > echoBuffery[0]  ) {
     distancey = ( echoBuffery[1] - echoBuffery[0] ) / 29 / 2 ;
  } else {
    distancey = 1000;
  }

  detachInterrupt(0);
  detachInterrupt(1);

  if ( distancex >= distancey ) {
    return distancey;
  } else {
    return distancex;
  }

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


void echo1_Interrupt_Handler() {
  echoBufferx[x++] = micros();
}

void echo2_Interrupt_Handler() {
  echoBuffery[y++] = micros();
}
