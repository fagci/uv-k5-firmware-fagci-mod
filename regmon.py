#!/usr/bin/env python3

from time import sleep
from libuvk5 import uvk5


PORT = '/dev/ttyUSB0'


with uvk5(PORT) as s:
    s.connect()
    s.get_fw_version()
    while True:
        print(s.get_reg(0x67))
        print(s.set_reg(0x13, 0b1010101010101010))
        print('---')
            
        sleep(1)

