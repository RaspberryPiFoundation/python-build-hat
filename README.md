Build and install
------------------

At the moment this only builds for test on a desktop (see the FakeHat
repo for the other end of the comms line).  Cross-compilation for the
Raspberry Pi will come in due course; let's get something working
first, OK?

To build the hub package, first create yourself a virtual environment
(if you haven't already) and turn it on:

```
$ python3 -m venv hat_env
$ source hat_env/bin/activate
```

You may need to install the I2C development library:

```
$ sudo apt install libi2c-dev
```

Now use the setup.py script to build and install the module:

```
(hub_env) $ USE_DUMMY_I2C=1 ./setup.py build
...much wibbling...
(hub_env) $ USE_DUMMY_I2C=1 ./setup.py install
```

You should now be able to "import hub" in a Python3 script and have
the module available to you.

Optionally the code may be compiled with "DEBUG_I2C=1" to enable logging
of the I2C traffic on the hub.


Documentation
-------------

Instructions for regenerating the documentation can be found in
docs/README.md.  Briefly, assuming you have the appropriate python
modules installed:

```
$ (cd docs; make html)
```

will rebuild the documentation.  The doc tree starts at
``docs/build/html/index.html``


Usage
-----

Aside from requiring

```python
from hub import hub
```

rather than

```python
import hub
```

this should be identical to normal usage you have connecting to a
Spike hub via a USB-serial terminal.  Firmware upgrade (and the
hub.Firmware class) is different; see its specific documentation for
details.


To control a motor attached to port A:

```python
from hub import hub

hub.port.A.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
hub.port.A.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
hub.port.A.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
hub.port.A.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
hub.port.A.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
hub.port.A.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
```

To rotate motor attached to port once clockwise when a button attached to port B is pressed:

```python
from hub import hub
import time

while True:
	if hub.port.B.device.get() > 0: # test if button is pressed
		hub.port.A.motor.run_for_degrees(360, 127) # turn 360 degrees clockwise at maximum speed
		time.sleep(0.5) # Wait half a second for motor to finish turning

```
