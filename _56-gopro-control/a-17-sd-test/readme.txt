*** read sd card of gopro, while gopro is on and not used by gopro

* Use SD to Micro SD Converter Connector.
esp-12 to microsd convertor
MISO --> MISO
MOSI --> MOSI
SCK  --> SCK
GND  --> GND
SS(one of 1/3/4/5) ---> SS

* use one of gpio 1/3/4/5 as SS(not 0/2/5 that related to boot selection)
* SD is powered by gopro(not by esp-12)
* sd size < 32GB, and FAT32
* esp-12 is powered by external power source

* todo : 
- add hero bus connector to turn on/off go pro(currently by wifi)
- add ds3231 rtc
