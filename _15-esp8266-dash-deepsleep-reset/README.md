#### button using esp8266/Arduino ####

- deepSleep(0)
- Using GPIO16 and NPN TR, reset is not permitted while device is running
- Use static ip for fast WIFI connection
- 22K R is changed to 330K

- WiFiClient::setLocalPortStart(micros() + vdd);
- wifi_set_phy_mode(PHY_MODE_11N);
- system_phy_set_rfoption(1);
- wifi_set_channel(channel);


![1](./pics/01.reset_switch_schem.png)

![2](./pics/buttonmillis.png)

![3](./pics/npntr.png)

![4](./pics/FullSizeRender%205.jpg)
