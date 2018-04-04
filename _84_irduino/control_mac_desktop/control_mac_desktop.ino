#include <IRremote.h>†
#include "Keyboard.h"

#define IR_CODE_SE  0x77E1BAD8
#define IR_CODE_UP  0x77E1D0D8
#define IR_CODE_DN  0x77E1B0D8
#define IR_CODE_LF  0x77E110D8
#define IR_CODE_RG  0x77E1E0D8
#define IR_CODE_MN  0x77E140D8
#define IR_CODE_PL  0x77E17AD8

int RECV_PIN = 6;

IRrecv irrecv(RECV_PIN);
decode_results results;

void setup() {
  Serial.begin(115200);
  Serial.println("Enabling IRin");
  irrecv.enableIRIn();
  Serial.println("Enabled IRin");
  Keyboard.begin();  
}

void send_key(int32_t irCode) {
   
  if(irCode<=0) return;
  if(irCode == 0xFFFFFFFF) return;
  if(irCode == 0x20D8) return;

  /*
  Serial.println(irCode);
  Serial.println(irCode, HEX);
  */
  
  switch(irCode) {
    case IR_CODE_SE:
      Serial.println("select");
      break;
    case IR_CODE_UP:
      Serial.println("up");
      break;    
    case IR_CODE_DN:
      Serial.println("down");
      break;    
    case IR_CODE_LF:
      Serial.println("left");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_LEFT_ARROW);
      delay(100);
      Keyboard.releaseAll();
      break;
    case IR_CODE_RG:
      Serial.println("right");
      Keyboard.press(KEY_LEFT_CTRL);
      Keyboard.press(KEY_RIGHT_ARROW);
      delay(100);
      Keyboard.releaseAll();      
      break;    
    case IR_CODE_MN:
      Serial.println("menu");
      break;    
    case IR_CODE_PL:
      Serial.println("play");
      break;

    default:
    break;
  }
}

void loop() {
  if (irrecv.decode(&results)) {
    int32_t irCode = results.value;
    send_key(irCode);
    irrecv.resume();
    delay(100);
  }
}
