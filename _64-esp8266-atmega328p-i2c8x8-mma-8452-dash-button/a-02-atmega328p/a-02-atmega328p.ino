// Clock internal 8MHz / Board : Atmega328 on breadboard
#include <LowPower.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/power.h>
#include <Wire.h>
#include <MMA8452.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

#define I2C_SLAVE_ADDR  0x26            // i2c slave address (38, 0x26)
#define I2C_MATRIX_ADDR 0x70

#define HT16K33_SS            B00100000 // System setup register
#define HT16K33_SS_STANDBY    B00000000 // System setup - oscillator in standby mode
#define HT16K33_SS_NORMAL     B00000001 // System setup - oscillator in normal mode

#define I2C_VCC 12
#define ESP_RST 17
#define BUTTON_INT 2
#define MMA_INT 3

Adafruit_8x8matrix matrix = Adafruit_8x8matrix();
MMA8452 accelerometer;

static const uint8_t PROGMEM
smile_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10100101,
  B10011001,
  B01000010,
  B00111100
},
neutral_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10111101,
  B10000001,
  B01000010,
  B00111100
},
frown_bmp[] =
{ B00111100,
  B01000010,
  B10100101,
  B10000001,
  B10011001,
  B10100101,
  B01000010,
  B00111100
},
light[] =
{ B00111100,
  B01000010,
  B10000101,
  B10001001,
  B10010001,
  B01100110,
  B00011000,
  B00011000
},
ac[] =
{ B01111111,
  B11000000,
  B10111110,
  B10100000,
  B10100111,
  B10110100,
  B11000000,
  B01111111
},
act[] =
{ B01111111,
  B11000000,
  B10111110,
  B10100000,
  B10100111,
  B10110010,
  B11000010,
  B01111111
},
large_heart[] =
{ B00000000,
  B01100110,
  B11111111,
  B11111111,
  B01111110,
  B00111100,
  B00011000,
  B00000000
},
small_heart[] =
{ B00000000,
  B00000000,
  B00100100,
  B01011010,
  B01000010,
  B00100100,
  B00011000,
  B00000000
},
fail_heart[] =
{ B00000000,
  B01100110,
  B10011101,
  B10001001,
  B01010010,
  B00100100,
  B00011000,
  B00000000
};

uint8_t HT16K33_i2c_write(uint8_t val) {
  Wire.beginTransmission(I2C_MATRIX_ADDR);
  Wire.write(val);
  return Wire.endTransmission();
} // _i2c_write

// Put the chip to sleep
//
uint8_t HT16K33_sleep() {
  return HT16K33_i2c_write(HT16K33_SS | HT16K33_SS_STANDBY); // Stop oscillator
} // sleep

/****************************************************************/
// Wake up the chip (after it been a sleep )
//
uint8_t HT16K33_normal() {
  return HT16K33_i2c_write(HT16K33_SS | HT16K33_SS_NORMAL); // Start oscillator
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect.
  }

  Serial.println("Starting....");

  pinMode(I2C_VCC, OUTPUT);
  digitalWrite(I2C_VCC, HIGH);
  delay(200);

  accelerometer.init();
  /*
    accelerometer.setDataRate(MMA_800hz); // we need a quick sampling rate
    accelerometer.setRange(MMA_RANGE_2G);
    accelerometer.enableSingleTapDetector(MMA_X);
    accelerometer.enableDoubleTapDetector(MMA_X, 0x22, 0xCC);
    accelerometer.setTapThreshold(0x55, 0x55, 0x33);
  */

  /*
    accelerometer.setDataRate(MMA_1_56hz);
    accelerometer.setRange(MMA_RANGE_2G);
    accelerometer.enableOrientationChange(true);
  */

  accelerometer.setDataRate(MMA_400hz);
  accelerometer.setRange(MMA_RANGE_2G);

  accelerometer.setMotionDetectionMode(MMA_MOTION, MMA_ALL_AXIS);
  accelerometer.setMotionTreshold(0x11);


  matrix.begin(I2C_MATRIX_ADDR);
  matrix.setRotation(3);


  matrix.clear();
  matrix.drawRect(3, 3, 2, 2, LED_ON);
  matrix.writeDisplay();
  delay(100);

  matrix.drawRect(2, 2, 4, 4, LED_ON);
  matrix.writeDisplay();
  delay(100);

  matrix.drawRect(1, 1, 6, 6, LED_ON);
  matrix.writeDisplay();
  delay(100);

  matrix.drawRect(0, 0, 8, 8, LED_ON);
  matrix.writeDisplay();
  delay(100);
  matrix.clear();
  matrix.writeDisplay();
}

void loop() {
  /*
    bool singleTap;
    bool doubleTap;
    bool x;
    bool y;
    bool z;
    bool negX;
    bool negY;
    bool negZ;
    accelerometer.getTapDetails(&singleTap, &doubleTap, &x, &y, &z, &negX, &negY, &negZ);

    if (singleTap || doubleTap) {

    Serial.print(millis());
    Serial.print(F(": "));

    if (doubleTap) Serial.print(F("Double"));
    Serial.print(F("Tap on "));

    if (x) {
      Serial.print(F("X "));
      Serial.print(negX ? F("left ") : F("right "));

      matrix.clear();      // clear display
      matrix.drawRect(0, 0, 8, 8, LED_ON);
      matrix.writeDisplay();  // write the changes we just made to the display
    }

    if (y) {
      Serial.print(F("Y "));
      Serial.print(negY ? F("down ") : F("up "));
      matrix.clear();      // clear display
      matrix.drawRect(1, 1, 6, 6, LED_ON);
      matrix.writeDisplay();  // write the changes we just made to the display
    }

    if (z) {
      Serial.print(F("Z "));
      Serial.print(negZ ? F("out ") : F("in "));
      matrix.clear();      // clear display
      matrix.drawRect(2, 2, 4, 4, LED_ON);
      matrix.writeDisplay();  // write the changes we just made to the display
    }
    Serial.println();
    }
  */

  /*
    bool orientationChanged;
    bool zTiltLockout;
    mma8452_orientation_t orientation;
    bool back;

    accelerometer.getPortaitLandscapeStatus(&orientationChanged, &zTiltLockout, &orientation, &back);

    if (orientationChanged) {
    Serial.print("Orientation is now ");
    switch (orientation)  {
      case MMA_PORTRAIT_UP:
        Serial.print(F("Portrait up"));
        break;
      case MMA_PORTRAIT_DOWN:
        Serial.print(F("Portrait down"));
        break;
      case MMA_LANDSCAPE_RIGHT:
        Serial.print(F("Landscape right"));
        break;
      case MMA_LANDSCAPE_LEFT:
        Serial.print(F("Landscape left"));
        break;
    }
    Serial.print(F(" (back: "));
    Serial.print(back ? F("yep )") : F("nope)"));
    Serial.print(F(" - Z Tilt lockout: "));
    Serial.println(zTiltLockout);
    }
  */

  bool motion = accelerometer.motionDetected();
  if (motion) {
    Serial.print(F("Motion @ "));
    Serial.println(millis());

    matrix.clear();
    matrix.drawRect(3, 3, 2, 2, LED_ON);
    matrix.writeDisplay();
    delay(30);

    matrix.drawRect(2, 2, 4, 4, LED_ON);
    matrix.writeDisplay();
    delay(50);

    matrix.drawRect(1, 1, 6, 6, LED_ON);
    matrix.writeDisplay();
    delay(60);

    matrix.drawRect(0, 0, 8, 8, LED_ON);
    matrix.writeDisplay();
    delay(100);
    matrix.clear();
    matrix.writeDisplay();
  }
}
