// https://github.com/bogde/HX711
#include "HX711.h"

#include <avr/interrupt.h>
#include <avr/sleep.h>

#include <SoftwareSerial.h>
const int rx = -1;
const int tx = 0;

// HX711.DOUT	- pin #A1
// HX711.PD_SCK	- pin #A0
// sck, DT
HX711 scale(3, 4);		// parameter "gain" is ommited; the default value 128 is used by the library
SoftwareSerial mySerial(rx, tx);

void setup() {

  pinMode(rx, INPUT);
  pinMode(tx, OUTPUT);
  mySerial.begin(9600);
  
  mySerial.println("HX711 Demo");

  mySerial.println("Before setting up the scale:");
  mySerial.print("read: \t\t");
  mySerial.println(scale.read());			// print a raw reading from the ADC

  mySerial.print("read average: \t\t");
  mySerial.println(scale.read_average(20));  	// print the average of 20 readings from the ADC

  mySerial.print("get value: \t\t");
  mySerial.println(scale.get_value(5));		// print the average of 5 readings from the ADC minus the tare weight (not set yet)

  mySerial.print("get units: \t\t");
  mySerial.println(scale.get_units(5), 1);	// print the average of 5 readings from the ADC minus tare weight (not set) divided 
						// by the SCALE parameter (not set yet)  

  scale.set_scale(22.f);                      // this value is obtained by calibrating the scale with known weights; see the README for details
  //scale.set_scale();
  scale.tare();				        // reset the scale to 0

  mySerial.println("After setting up the scale:");

  mySerial.print("read: \t\t");
  mySerial.println(scale.read());                 // print a raw reading from the ADC

  mySerial.print("read average: \t\t");
  mySerial.println(scale.read_average(20));       // print the average of 20 readings from the ADC

  mySerial.print("get value: \t\t");
  mySerial.println(scale.get_value(5));		// print the average of 5 readings from the ADC minus the tare weight, set with tare()

  mySerial.print("get units: \t\t");
  mySerial.println(scale.get_units(5), 1);        // print the average of 5 readings from the ADC minus tare weight, divided 
						// by the SCALE parameter set with set_scale

  mySerial.println("Readings:");
}

void loop() {
  mySerial.print("one reading:\t");
  mySerial.print(scale.get_units(), 1);
  mySerial.print("\t| average:\t");
  mySerial.println(scale.get_units(10), 1);

  //scale.power_down();			        // put the ADC in sleep mode
  delay(100);
  //scale.power_up();
}
