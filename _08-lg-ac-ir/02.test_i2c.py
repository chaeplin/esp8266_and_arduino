# 02.test_i2c.py 

import smbus
import time
bus = smbus.SMBus(1)

address = 0x07

def writeNumber(a, b ):
    bus.write_i2c_block_data(address, a, [b])


writeNumber(25, 2)
