Welcome
-------

This Python module allows you to run the Raspberry Pi Build HAT.  It
includes detailed documentation -- see below for how to generate and
read it.

The ``build_hat`` module provides a ``BuildHAT`` class that holds
information on all of the devices attached to the HAT.  Individual
motors and sensors can be found under ``bh.port.X`` where ``X`` is the
name of the port and ``bh`` is an instance ``BuildHAT``.  In addition
there are callbacks to inform the code that devices have been attached
to or detached from the HAT, status functions to determine exactly
what device is on a port, and specialist functions to drive motors.


Build and install
------------------

To build the hub package, first create yourself a virtual environment
(if you haven't already) and turn it on:

```
$ python3 -m venv hat_env
$ source hat_env/bin/activate
```

Now use the setup.py script to build and install the module:

```
(hub_env) $ ./setup.py build
...much wibbling...
(hub_env) $ ./setup.py install
```

You should now be able to "import hub" in a Python3 script and have
the module available to you.

Optionally the code may be compiled with "DEBUG\_UART=1" to enable logging
of the UART traffic on the hub.

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

Programming Bootloader
----------------------

* Copy pico\_setup.sh to ~/Repositories and run (this will install openocd along with tools for building the firmware)

* Use the following command to program the bootloader:

```
openocd -f ~/Repositories/pico/openocd/tcl/interface/raspberrypi-swd.cfg -f ~/Repositories/pico/openocd/tcl/target/rp2040.cfg -c "program bootloader.elf verify reset exit"
```

* Currently need to place firmware.bin and signature.bin in /tmp for the Python library to load
