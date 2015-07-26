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

unsigned long  tv_input = 0x20DFD02F;
unsigned long  tv_right = 0x20DF609F;
unsigned long  tv_left  = 0x20DFE01F;
unsigned long  tv_enter = 0x20DF22DD;

int tv_cur_input_no  = 1;
int tv_move_input_no = 3;

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

void tv_input_to_blank(){
  irsend.sendNEC(tv_input, 32);
  delay(3000);
  for ( int i = 0 ; i < ( tv_move_input_no - tv_cur_input_no) ; i++ ) {
    irsend.sendNEC(tv_right, 32);
    delay(300);
  }
  irsend.sendNEC(tv_enter, 32);
}

void tv_input_to_cur(){
  irsend.sendNEC(tv_input, 32);
  delay(3000);
  for ( int i = 0 ; i < ( tv_move_input_no - tv_cur_input_no) ; i++ ) {
    irsend.sendNEC(tv_left, 32);
    delay(300);
  }
  irsend.sendNEC(tv_enter, 32);
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
    Serial.println("to blank ch");
    tv_input_to_blank();
    delay(50);
    power_status = 0;
  }

  if ( ( ave.stddev() < 10) && ( int(ave.mean()) > 100 )  && ( power_status == 0) ) {
    Serial.println("to cur ch");
    tv_input_to_cur();
    delay(50);
    power_status = 1;
  }

  delay(500); 
}
