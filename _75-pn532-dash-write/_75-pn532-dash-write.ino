// card : elechouse pn532 card
// lib : https://github.com/elechouse/PN532
// example from : https://github.com/don/NDEF/blob/master/examples/WriteTag/WriteTag.ino
#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_SPI pn532spi(SPI, 5);
NfcAdapter nfc = NfcAdapter(pn532spi);

void setup() {
      Serial.begin(115200);
      Serial.println("NDEF Writer");
      nfc.begin();
}

void loop() {
    Serial.println("\nPlace a formatted Mifare Classic NFC tag on the reader.");
    if (nfc.tagPresent()) {
        NdefMessage message = NdefMessage();
        // Amanda B. Johnson's
        message.addUriRecord("dash:XuhHHnjLbm559kzByxtQ6gHMpjs32MHKb2?amount=0.01");

        bool success = nfc.write(message);
        if (success) {
          Serial.println("Success. Try reading this tag with your phone.");        
        } else {
          Serial.println("Write failed.");
        }
    }
    delay(5000);
}
