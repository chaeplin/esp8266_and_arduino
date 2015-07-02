door alarm using pro mini as external reset
===========================================

- battery powered
- mqtt pub : door status, voltage
- esp8266 : deep sleep, pro mini : sleep with power down


* pro mini pin

| pin | to                                   |
|---- | ------------------------------------ |
|  2  | INPUT_PULLUP, magnetic door switch   |
|  5  | buzzer vcc                           |
|  6  | buzzer i/o                           |
|  7  | esp8266 gpio 5                       |
|  8  | esp8266 gpio 16                      |
|  9  | esp8266 reset                        |


![1](https://raw.githubusercontent.com/chaeplin/esp8266_and_arduino/master/_01-door-alarm/pics/01.png)

![2](https://raw.githubusercontent.com/chaeplin/esp8266_and_arduino/master/_01-door-alarm/pics/02.png)