# 02.test_i2c.py 

import smbus
import time
bus = smbus.SMBus(1)

address = 0x07

def writeNumber(a, b ):
    bus.write_i2c_block_data(address, a, [b])


with open('data/NemoWeight1.txt') as temp_file:
  drugs = [line.rstrip('\n') for line in temp_file]


for x in drugs:
    if int(x) > 1000 :
       print x
       m = ( int(x) >> 8 ) & 0xFF
       y = int(x) & 0xFF
       writeNumber(m, y)
       time.sleep(0.2)

       