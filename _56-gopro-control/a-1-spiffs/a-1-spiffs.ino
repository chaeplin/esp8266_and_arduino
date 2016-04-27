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


  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.print(dir.fileName());
    File f = dir.openFile("r");
    Serial.println(f.size());
  }


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

  Serial.println(millis());
  Serial.println();
}

void loop() {
  // nothing to do for now, this is just a simple test

}
