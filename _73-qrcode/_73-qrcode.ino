/* qrcode test using https://github.com/anunpanya/ESP8266_QRcode 
 * using nodemcu V1  
 *  D3 : SDA : GPIO 0
 *  D4 : SCK : GPIO 2
 */ 
/* *********************************************************************************
 * ESP8266 QRcode 
 * dependency library :
 *   ESP8266 Oled Driver for SSD1306 display by Daniel Eichborn, Fabrice Weinberg
***********************************************************************************/


#include <qrcode.h>
#include <SSD1306.h>

SSD1306 display(0x3c, 0, 2);
QRcode qrcode (&display);

/*
#include <qrcode.h>
#include <brzo_i2c.h>
#include "SSD1306Brzo.h"

SSD1306Brzo display(0x3c, 0, 2);
QRcode qrcode (&display);
*/

void setup() {

    Serial.begin(115200);
    Serial.println("");
    Serial.println("Starting...");

    display.init();
    display.clear();
    display.display();


    // enable debug qrcode
    // qrcode.debug();

    // Initialize QRcode display using library
    qrcode.init();
    // create qrcode
    // https://www.youtube.com/watch?v=DmsfFfD4mQo
    qrcode.create("dash:XxdaVbaPzXm8ow7zZbUeEYftAYpuZhairZ?amount=0.01&instatsend=1");

}

void loop() { }
