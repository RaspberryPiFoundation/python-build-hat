from signal import pause
from buildhat import Motor, DistanceSensor

motor = Motor('C')
dist = DistanceSensor('B')

dist.wait_until_distance_closer_cm(20)
motor.run_for_rotations(1) 

dist.wait_until_distance_farther_cm(40)
motor.run_for_rotations(2)

def handle_dist(dist):
    print("distance ",dist)

dist.when_motion = handle_dist
pause()
