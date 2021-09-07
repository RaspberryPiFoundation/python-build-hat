Welcome
-------

This Python module allows you to utilise the Raspberry Pi Build HAT.  It
includes detailed documentation -- see below for how to generate and
read it.

Install
-------

If using asdf:

```
asdf install
```

Then:

```
pip3 install . --user
```

Build
-----

```
./build.sh
```

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

```
sudo apt install automake autoconf build-essential texinfo libtool libftdi-dev libusb-1.0-0-dev
git clone https://github.com/raspberrypi/openocd.git --recursive --branch rp2040 --depth=1
cd openocd
./bootstrap
./configure --enable-ftdi --enable-sysfsgpio --enable-bcm2835gpio
make -j4
sudo make install
```

* Use the following command to program the bootloader:

```
openocd -s /usr/local/share/openocd/scripts -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program bootloader.elf verify reset exit"
```

Install
-------

```
pip3 install buildhat-*.whl
```
