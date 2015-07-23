# 02.test_i2c.py 

import smbus
import time
bus = smbus.SMBus(1)

address = 0x07

def writeNumber(a, b ):
    bus.write_i2c_block_data(address, a, [b])


writeNumber(25, 2)

# writeNumber(1, 0)
# writeNumber(0, 0)
# writeNumber(2, 1)
# writeNumber(3, 1)
# writeNumber(4, 2)
# writeNumber(5, 26)
# 
# 
# a : mode or temp 		b : air_flow, temp, swing, clean
# 18 ~ 30 : temp		0 ~ 3 : flow
# 0 : off				0
# 1 : on				0
# 2 : air_swing			0 or 1
# 3 : air_clean			0 or 1
# 4 : air_flow			0 ~ 3 : flow
# 5 : temp				18 ~ 30
