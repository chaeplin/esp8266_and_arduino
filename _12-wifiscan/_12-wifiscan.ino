/*
 *  This sketch demonstrates how to scan WiFi networks.
 *  The API is almost the same as with the WiFi Shield library,
 *  the most obvious difference being the different file you need to include:
 */
#include "ESP8266WiFi.h"

void setup() {
  Serial.begin(115200);

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("Setup done");
}

void loop() {
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0)
    Serial.println("no networks found");
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    Serial.println("no\tRSSI\tenc\tBSSID\t\t\tCH\th\tSSID");
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      if ( i < 9)
        Serial.print("0");
      Serial.print(i + 1);
      Serial.print("\t");
      Serial.print(WiFi.RSSI(i));
      Serial.print("\t");
      Serial.print(WiFi.encryptionType(i));
      Serial.print((WiFi.encryptionType(i) == ENC_TYPE_NONE)?" ":"*");
      Serial.print("\t");
      Serial.print(WiFi.BSSIDstr(i));
      Serial.print("\t");
      Serial.print(WiFi.channel(i));
      Serial.print("\t");
      Serial.print(WiFi.isHidden(i));
      Serial.print("\t");
      Serial.println(WiFi.SSID(i));
      delay(10);
    }
  }
  Serial.println("");

  // Wait a bit before scanning again
  delay(5000);
}
