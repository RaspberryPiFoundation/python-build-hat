"""Example for pair of motors"""

from buildhat import MotorPair

pair = MotorPair('C', 'D')
pair.set_default_speed(20)
pair.run_for_rotations(2)

pair.run_for_rotations(1, speedl=100, speedr=20)

pair.run_to_position(20, 100, speed=20)
