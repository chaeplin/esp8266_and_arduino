/*
https://github.com/JRodrigoTech/Ultrasonic-HC-SR04
https://github.com/MajenkoLibraries/Average
*/
#include <Average.h>
#include <Ultrasonic.h>

Ultrasonic ultraleft(8,9,30000);   // (Trig PIN,Echo PIN)
Ultrasonic ultraright(6,7,30000);  // (Trig PIN,Echo PIN)

Average<float> ave(5);

void setup() {
  Serial.begin (38400);
  pinMode(Trig_pin, OUTPUT);
  pinMode(2, INPUT_PULLUP);
}

long check_distance() {
  int distanceleft = ultraleft.Ranging(CM);
  delay(50);
  int distanceright = ultraright.Ranging(CM);
  delay(50);

  if ( distanceleft >= distanceright ) {
     return distanceright;
  } else {
     return distanceleft ;
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
