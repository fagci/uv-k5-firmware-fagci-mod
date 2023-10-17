#!/usr/bin/env python3

from serial import Serial
from time import sleep

SPEED = 38400
PORT = '/dev/ttyUSB0'

with Serial(PORT, SPEED, timeout=3) as s:
    while True:
        s.write(b'RR\x67;')
        print(s.read_until())
        
        sleep(1)

