"""Example using motion sensor"""

from time import sleep

from buildhat import MotionSensor

motion = MotionSensor('A')

for _ in range(50):
    print(motion.get_distance())
    sleep(0.1)
