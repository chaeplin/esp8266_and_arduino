* ESP8266 mqtt node

#### Server ####
- mqtt broker : mosquitto
- db : influxdb
- web : grafana, node-red
- collect : mqtt2graphite
- alert : mqttwarn 


### Library ###
- https://github.com/knolleary/pubsubclient
- https://github.com/milesburton/Arduino-Temperature-Control-Library
- https://github.com/adafruit/DHT-sensor-library
- https://github.com/bogde/HX711
- https://github.com/Makuna/Rtc
- LiquidCrystal_I2C.h
- https://github.com/openenergymonitor/EmonLib
- https://github.com/bblanchon/ArduinoJson
- https://github.com/MajenkoLibraries/Average
- https://github.com/Yveaux/arduino_vcc
- https://github.com/PaulStoffregen/Time
- https://github.com/z3t0/Arduino-IRremote



#### _04-mqtt-lcd-dust-sensor ####
- esp-01 +(i2c) nano
- lcd(i2c), rtc(i2c), level convertor, Sharp dust sensor, IR sned/recv
- display clock, temp/humidity(mqtt sub), dust
- control LG_AC using Apple remote
