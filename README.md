# Build HAT

The Python Build HAT library supports the Raspberry Pi Build HAT, an add-on board for the Raspberry Pi computer, which allows control of up to four LEGO® TECHNIC™ motors and sensors included in the SPIKE™ Portfolio.

## Hardware

The Build HAT provides four connectors for LEGO® Technic™ motors and sensors from the SPIKE™ Portfolio. The available sensors include a distance sensor, a colour sensor, and a versatile force sensor. The angular motors come in a range of sizes and include integrated encoders that can be queried to find their position.

The Build HAT fits all Raspberry Pi computers with a 40-pin GPIO header, including — with the addition of a ribbon cable or other extension device — Raspberry Pi 400. Connected LEGO® Technic™ devices can easily be controlled in Python, alongside standard Raspberry Pi accessories such as a camera module.

## Documentation

Library documentation: https://buildhat.readthedocs.io

Hardware documentation: https://www.raspberrypi.com/documentation/accessories/build-hat.html

Projects and inspiration: https://projects.raspberrypi.org/en/pathways/lego-intro

## Installation

To install the Build HAT library, enter the following commands in a terminal:

    pip3 install buildhat

## Usage

See the [detailed documentation](https://buildhat.readthedocs.io/en/latest/buildhat/index.html) for the Python objects available.

```python
import time
from signal import pause
from buildhat import Motor

motor = Motor('A')
motor.set_default_speed(30)

print("Position", motor.get_aposition())

def handle_motor(speed, pos, apos):
    print("Motor", speed, pos, apos)

motor.when_rotated = handle_motor

print("Run for degrees")
motor.run_for_degrees(360)

print("Run for seconds")
motor.run_for_seconds(5)

print("Run for rotations")
motor.run_for_rotations(2)

print("Start motor")
motor.start()
time.sleep(3)
print("Stop motor")
motor.stop()

pause()
```

## Building locally

Using [asdf](https://github.com/asdf-vm/asdf):

```
asdf install
```

Then:

```
pip3 install . --user
```

### Building the documentation

Instructions for regenerating the documentation can be found in
`docs/README.md`. Briefly, assuming you have the appropriate python
modules installed:

```
$ (cd docs; make html)
```

will rebuild the documentation. The doc tree starts at `docs/build/html/index.html`
