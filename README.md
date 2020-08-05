Welcome
-------

This Python module allows you to run the Raspberry Pi Build HAT.  It
includes detailed documentation -- see below for how to generate and
read it.

The ``hub`` module provides a ``hub`` object that holds information on
all of the devices attached to the HAT.  Individual motors and sensors
can be found under ``hub.port.X`` where ``X`` is the name of the
port.  In addition there are callbacks to inform the code that devices
have been attached to or detached from the HAT, status functions to
determine exactly what device is on a port, and specialist functions
to drive motors.


Build and install
------------------

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
(hub_env) $ ./setup.py build
...much wibbling...
(hub_env) $ ./setup.py install
```

You should now be able to "import hub" in a Python3 script and have
the module available to you.

Optionally the code may be compiled with "DEBUG\_I2C=1" to enable logging
of the I2C traffic on the hub.

The code may be compiled with "USE\_DUMMY\_I2C=1" to use an ethernet local
loopback as a fake I2C for test purposes.


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

See the detailed documentation for the Python objects available.

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
