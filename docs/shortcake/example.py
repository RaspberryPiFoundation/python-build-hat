import time
from signal import pause
from buildhat import Motor, DistanceSensor, ForceSensor

motor = Motor('C')
button = ForceSensor('A')
dist = DistanceSensor('B')

button.wait_until_pressed()
motor.run_for_rotations(1)

dist.wait_until_distance_closer_cm(20)
motor.run_for_rotations(2) 

def handle_motor(speed, pos):
    if speed < -4:
        print("left rotation")
    elif speed > 4:
        print("right rotation")

motor.when_rotated = handle_motor
pause()
