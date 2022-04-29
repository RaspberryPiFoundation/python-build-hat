"""Example for distance sensor"""

from signal import pause

from buildhat import DistanceSensor, Motor

motor = Motor('A')
dist = DistanceSensor('D', threshold_distance=100)

print("Wait for in range")
dist.wait_for_in_range(50)
motor.run_for_rotations(1)

print("Wait for out of range")
dist.wait_for_out_of_range(100)
motor.run_for_rotations(2)


def handle_in(distance):
    """Within range

    :param distance: Distance
    """
    print("in range", distance)


def handle_out(distance):
    """Out of range

    :param distance: Distance
    """
    print("out of range", distance)


dist.when_in_range = handle_in
dist.when_out_of_range = handle_out
pause()
