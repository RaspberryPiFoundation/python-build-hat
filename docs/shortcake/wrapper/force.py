from signal import pause
from buildhat import Motor, ForceSensor

motor = Motor('C')
button = ForceSensor('A')

button.wait_until_pressed()
motor.run_for_rotations(1)

button.wait_until_released()

button.wait_until_pressed()
motor.run_for_rotations(2) 

def handle_force(force, pressed):
    print("Force", force, pressed)

button.when_force = handle_force
pause()
