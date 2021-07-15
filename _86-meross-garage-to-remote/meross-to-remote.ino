
// INPUT
#define REED_SENSOR_PIN 14
#define HKIT_CMD_PIN 10
#define HKIT_GND_PIN 16
 
// OUTPUT
#define AWNING_OPEN_CMD_PIN 8
#define AWNING_CLOSE_CMD_PIN 9
 
 
bool AWNING_REEDS_STATUS;
 
unsigned long DURATION;
 
void setup() {
  pinMode(REED_SENSOR_PIN, INPUT);
  pinMode(HKIT_CMD_PIN, INPUT_PULLUP);
  pinMode(HKIT_GND_PIN, OUTPUT);
 
  pinMode(AWNING_OPEN_CMD_PIN, OUTPUT);
  pinMode(AWNING_CLOSE_CMD_PIN, OUTPUT);
 
  digitalWrite(HKIT_GND_PIN, LOW);
  digitalWrite(AWNING_OPEN_CMD_PIN, LOW);
  digitalWrite(AWNING_CLOSE_CMD_PIN, LOW);
}
 
void loop() {
  DURATION = pulseIn(HKIT_CMD_PIN, LOW, 1000000000);
  if ( DURATION > 10 ) {
    AWNING_REEDS_STATUS = digitalRead(REED_SENSOR_PIN);
    if(AWNING_REEDS_STATUS) { // CLOSED STATUS --> OPEN
      digitalWrite(AWNING_OPEN_CMD_PIN, HIGH);
      delay(500);
      digitalWrite(AWNING_OPEN_CMD_PIN, LOW);
      Serial.println("CMD IN, OPENNING AWNING");      
    } else { 
      digitalWrite(AWNING_CLOSE_CMD_PIN, HIGH);
      delay(500);
      digitalWrite(AWNING_CLOSE_CMD_PIN, LOW);
      Serial.println("CMD IN, CLOSING AWNING");         
    }
  }
}
