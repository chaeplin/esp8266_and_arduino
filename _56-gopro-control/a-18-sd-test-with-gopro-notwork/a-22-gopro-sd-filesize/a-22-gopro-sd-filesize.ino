/*
  SD card file dump

  This example shows how to read a file from the SD card using the
  SD library and send it over the serial port.

  The circuit:
   SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4

  created  22 December 2010
  by Limor Fried
  modified 9 Apr 2012
  by Tom Igoe

  This example code is in the public domain.

*/

#include <SPI.h>
#include <SD.h>

const int chipSelect = 5;
String myFile = "DCIM/101GOPRO/GOPR2478.JPG";

bool x;

void setup()
{

  x = true;
  // Open serial communications and wait for port to open:
  Serial.begin(74880);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }


  Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    return;
  }
  Serial.println("card initialized.");

}

void loop()
{
  if (x) {
    // open the file. note that only one file can be open at a time,
    // so you have to close this one before opening another.
    File dataFile = SD.open(myFile);
    // if the file is available, write to it:
    if (dataFile) {
      Serial.println(millis()/1000);
      Serial.print(myFile);
      Serial.print(" ----> ");
      Serial.println(dataFile.size());
      while (dataFile.available()) {
        //Serial.write(dataFile.read());
        dataFile.read();
        delay(0);
      }
      dataFile.close();
    }
    // if the file isn't open, pop up an error:
    else {
      Serial.println("can't open file");
    }
    Serial.println(); Serial.println();
    Serial.println("done");
    Serial.println(millis()/1000);
    x = false;
  }
}

