/*
https://github.com/JRodrigoTech/Ultrasonic-HC-SR04
https://github.com/MajenkoLibraries/Average
*/
#include <Average.h>
#include <Ultrasonic.h>
#include <IRremote.h>
IRsend irsend;

Ultrasonic ultraleft(5,6,30000);   // (Trig PIN,Echo PIN)
Ultrasonic ultraright(9,10,30000);  // (Trig PIN,Echo PIN)

Average<float> ave(5);

int power_status = 1 ;

void setup() {
  Serial.begin (38400);
  pinMode(4, OUTPUT); // VCC pin
  pinMode(7, OUTPUT); // GND ping

  pinMode(8, OUTPUT); // VCC pin
  pinMode(11, OUTPUT); // GND ping
   
  digitalWrite(4, HIGH); // VCC +5V mode  
  digitalWrite(7, LOW);  // GND mode  

  digitalWrite(8, HIGH); // VCC +5V mode  
  digitalWrite(11, LOW);  // GND mode  

  pinMode(2, OUTPUT); // GND for IR
  digitalWrite(2, LOW);
  
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

  if ( ( ave.stddev() < 10) && ( int(ave.mean()) <= 60 )  && ( power_status == 1) ) {
    Serial.println("tv off");
    irsend.sendNEC(0x20DF10EF, 32); // LG TV power code
    delay(50);
    power_status = 0;
  }

  if ( ( ave.stddev() < 10) && ( int(ave.mean()) > 100 )  && ( power_status == 0) ) {
    Serial.println("tv on");
    irsend.sendNEC(0x20DF10EF, 32); // LG TV power code
    delay(50);
    power_status = 1;
  }

  delay(500); 
}
