// pin

#define IRPIN 4

volatile long startMills;

volatile int IRSTATUS = LOW ;
int OLDIRSTATUS ;

void setup() {
  Serial.begin(38400);
  Serial.println("power meter test!");
  delay(20);
  
  startMills = millis();

  pinMode(IRPIN, INPUT_PULLUP);
  attachInterrupt(4, count_powermeter, FALLING); 

  OLDIRSTATUS = LOW ;
    
}

void loop()
{
  if ( IRSTATUS != OLDIRSTATUS ) 
  {
      Serial.print("ir read => ");
      Serial.println(millis() - startMills);
      startMills = millis();   
      OLDIRSTATUS = IRSTATUS ;
  }
}

void count_powermeter()
{
  /*
 if (( millis() - startMills ) < 700 ) {
       return;
 } else {
  startMills = millis();
  */
  IRSTATUS = !IRSTATUS ;
  /*
 }
 */
}

/*

void loop() {
  Serial.print("adc read => ");
  Serial.println(analogRead(A0));  
  delay(5000);
  
}


void count_powermeter() {
     if (( millis() - startMills ) < 500 ) {
       return;
     } else {
       Serial.print("\t\t\t\t\t ir read => ");
       Serial.println(millis() - startMills);
       startMills = millis();
     } 
}

*/
