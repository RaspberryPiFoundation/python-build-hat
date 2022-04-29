"""Example driving motors"""

import time

from buildhat import Motor

motor = Motor('A')
motorb = Motor('B')


def handle_motor(speed, pos, apos):
    """Motor data

    :param speed: Speed of motor
    :param pos: Position of motor
    :param apos: Absolute position of motor
    """
    print("Motor", speed, pos, apos)


motor.when_rotated = handle_motor
motor.set_default_speed(50)

print("Run for degrees 360")
motor.run_for_degrees(360)
time.sleep(3)

print("Run for degrees -360")
motor.run_for_degrees(-360)
time.sleep(3)

print("Start motor")
motor.start()
time.sleep(3)
print("Stop motor")
motor.stop()
time.sleep(1)

print("Run for degrees - 180")
motor.run_for_degrees(180)
time.sleep(3)

print("Run for degrees - 90")
motor.run_for_degrees(90)
time.sleep(3)

print("Run for rotations - 2")
motor.run_for_rotations(2)
time.sleep(3)

print("Run for seconds - 5")
motor.run_for_seconds(5)
time.sleep(3)

print("Run both")
motor.run_for_seconds(5, blocking=False)
motorb.run_for_seconds(5, blocking=False)
time.sleep(10)

print("Run to position -90")
motor.run_to_position(-90)
time.sleep(3)

print("Run to position 90")
motor.run_to_position(90)
time.sleep(3)

print("Run to position 180")
motor.run_to_position(180)
time.sleep(3)
