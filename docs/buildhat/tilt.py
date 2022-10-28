"""Example using tilt sensor"""

from time import sleep

from buildhat import TiltSensor

tilt = TiltSensor('A')

for _ in range(50):
    print(tilt.get_tilt())
    sleep(0.1)
