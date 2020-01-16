Build instructions:

At the moment this only builds for test on a desktop (see the FakeHat
repo for the other end of the comms line).  Cross-compilation for the
Raspberry Pi will come in due course; let's get something working
first, OK?

To build the hub package, first create yourself a virtual environment
(if you haven't already) and turn it on:

$ python3 -m venv hat_env
$ source hat_env/bin/activate


Now use the setup.py script to build and install the module:

(hub_env) $ USE_DUMMY_I2C=1 ./setup.py build
...much wibbling...
(hub_env) $ USE_DUMMY_I2C=1 ./setup.py install


You should now be able to "import hub" in a Python3 script and have
the module available to you.
