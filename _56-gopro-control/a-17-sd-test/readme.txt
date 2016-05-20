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

![1](./pics/IMG_5875.JPG)
![1](./pics/IMG_5877.JPG)
![1](./pics/IMG_5878.JPG)
![1](./pics/connector1.jpg)
![1](./pics/connector2.jpg)

![1](./pics/sd-pinout.jpg)
![1](./pics/esp12.png)

![1](./pics/ds3231.png)

![1](./pics/sd_current.png)
