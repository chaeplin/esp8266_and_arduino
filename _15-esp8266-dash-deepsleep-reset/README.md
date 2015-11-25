#### button using esp8266/Arduino ####

- deepSleep(0)
- Using GPIO16 and NPN TR, reset is not permitted while device is running
- Use static ip for fast WIFI connection
- 22K R is changed to 330K
- WiFi.disconnect() is used for consistent operation( under 5sec)
--> when device wake up, sometimes it takes more than 10 sec to connect wifi without WiFi.disconnect()


![1](https://raw.githubusercontent.com/chaeplin/esp8266_and_arduino/master/_15-esp8266-dash-deepsleep-reset/pics/npntr.png)

![2](https://raw.githubusercontent.com/chaeplin/esp8266_and_arduino/master/_15-esp8266-dash-deepsleep-reset/pics/FullSizeRender%205.jpg)
