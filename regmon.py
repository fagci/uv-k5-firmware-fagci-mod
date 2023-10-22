#!/usr/bin/env python3

from time import sleep
from libuvk5 import uvk5


PORT = '/dev/ttyUSB0'

vals = [
    0x1f80,
    0x2f50,
    0x9f30,
    0x3f48,
    0xaf28,
    0x4f44,
    0xbf24,
    0x5f42,
    0xcf22,
    0x6f41,
    0xdf21,
]

with uvk5(PORT) as s:
    s.connect()
    s.get_fw_version()
    i=0;
    while True:
        val = (i % 1000) *100
        print(val)
        s.set_reg(0x3D, val)
        print('---')
        i+=1
            
        sleep(2)

