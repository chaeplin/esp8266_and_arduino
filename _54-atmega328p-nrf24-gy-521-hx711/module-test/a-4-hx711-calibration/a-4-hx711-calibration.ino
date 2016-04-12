// https://github.com/bogde/HX711
#include "HX711.h"

#include <avr/interrupt.h>
#include <avr/sleep.h>

/*
How to Calibrate Your Scale

Call set_scale() with no parameter.
Call tare() with no parameter.
Place a known weight on the scale and call get_units(10).
Divide the result in step 3 to your known weight. You should get about the parameter you need to pass to set_scale.
Adjust the parameter in step 4 until you get an accurate reading.
*/

// HX711.DOUT	- pin #A1
// HX711.PD_SCK	- pin #A0
// sck, DT
HX711 scale(7, 8);		// parameter "gain" is ommited; the default value 128 is used by the library

void setup() {
  Serial.begin(115200);

  //scale.set_scale(23999.f);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  scale.set_scale(23500.f);
  //scale.set_scale();
  scale.tare();				        // reset the scale to 0

}

void loop() {
  Serial.print("one reading:\t");
  Serial.print(scale.get_units() * 1000, 0);
  Serial.print("\t| average:\t");
  Serial.println(scale.get_units(10) * 1000, 0);

  //scale.power_down();			        // put the ADC in sleep mode
  delay(1000);
  //scale.power_up();
}
