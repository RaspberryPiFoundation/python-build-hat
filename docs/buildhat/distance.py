from signal import pause
from buildhat import Motor, DistanceSensor

motor = Motor('A')
dist = DistanceSensor('D')

dist.wait_for_in_range(50)
motor.run_for_rotations(1) 

dist.wait_for_out_of_range(100)
motor.run_for_rotations(2)

def handle_dist(dist):
    print("distance ",dist)

dist.when_motion = handle_dist
pause()
