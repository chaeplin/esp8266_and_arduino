// http://blog.squix.ch/2015/08/esp8266arduino-playing-around-with.html

#include "FS.h"

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  Serial.println();
  Serial.println(millis());
  

  // always use this to "mount" the filesystem
  bool result = SPIFFS.begin();
  Serial.println("SPIFFS opened: " + result);

  // this opens the file "f.txt" in read-mode
  File f = SPIFFS.open("/f.txt", "r");

  if (!f) {
    Serial.println("File doesn't exist yet. Creating it");

    // open the file in write mode
    File f = SPIFFS.open("/f.txt", "w");
    if (!f) {
      Serial.println("file creation failed");
    }
    // now write two lines in key/value style with  end-of-line characters
    f.println("ssid=abc");
    f.println("password=123455secret");
  } else {
    // we could open the file
    while (f.available()) {
      //Lets read line by line from the file
      String line = f.readStringUntil('\n');
      Serial.println(line);
    }

  }
  //
  Serial.println();
  Serial.print("file.name(): "); Serial.println(f.name());
  Serial.print("file.size(): "); Serial.println(f.size());

  Serial.println();
  FSInfo info;
  if (!SPIFFS.info(info)) {
    Serial.println("info failed");
  } else {
    Serial.printf("Total: %u\nUsed: %u\nBlock: %u\nPage: %u\nMax open files: %u\nMax path len: %u\n",
                  info.totalBytes,
                  info.usedBytes,
                  info.blockSize,
                  info.pageSize,
                  info.maxOpenFiles,
                  info.maxPathLength
                 );
  }

  f.close(); 
  Serial.println(millis());
  Serial.println();
}

void loop() {
  // nothing to do for now, this is just a simple test

}
