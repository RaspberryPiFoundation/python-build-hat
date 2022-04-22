"""Example using force sensor"""

from signal import pause

from buildhat import ForceSensor, Motor

motor = Motor('A')
button = ForceSensor('D', threshold_force=1)

print("Waiting for button to be pressed fully and released")

button.wait_until_pressed(100)
button.wait_until_released(0)

motor.run_for_rotations(1)

print("Wait for button to be pressed")

button.wait_until_pressed()
motor.run_for_rotations(2)


def handle_pressed(force):
    """Force sensor pressed

    :param force: Force value
    """
    print("pressed", force)


def handle_released(force):
    """Force sensor released

    :param force: Force value
    """
    print("released", force)


button.when_pressed = handle_pressed
button.when_released = handle_released
pause()
