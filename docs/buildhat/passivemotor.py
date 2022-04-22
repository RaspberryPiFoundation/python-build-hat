"""Passive motor example"""

import time

from buildhat import PassiveMotor

motor = PassiveMotor('A')

print("Start motor")
motor.start()
time.sleep(3)
print("Stop motor")
motor.stop()
