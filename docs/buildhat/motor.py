from signal import pause
from buildhat import Motor

motor = Motor('C')
motor.run_for_rotations(1)

motor.run_to_position(0)

def handle_motor(speed, pos):
    print("Motor", speed, pos)

motor.when_rotated = handle_motor
motor.run_for_rotations(1)

pause()
