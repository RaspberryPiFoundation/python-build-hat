#! /usr/bin/env python3

from setuptools import setup, Extension
from os import getenv

hub_module = Extension('hub',
                       include_dirs = ['include'],
                       sources = ['src/hubmodule.c',
                                  'src/i2c.c',
                                  'src/queue.c',
                                  'src/cmd.c',
                                  'src/port.c',
                                  'src/device.c',
                                  'src/motor.c'])

# If the environment variable USE_DUMMY_I2C is set, build with a fake
# back end for testing on a desktop.
if getenv("USE_DUMMY_I2C") == "1":
    hub_module.sources.append('src/dummy-i2c.c')
    hub_module.define_macros.append(('USE_DUMMY_I2C', '1'))

# Similarly if DEBUG_I2C is set, extra commands are added to the hub
# module to facilite debugging.
if getenv("DEBUG_I2C") == "1":
    hub_module.sources.append('src/debug-i2c.c')
    hub_module.define_macros.append(('DEBUG_I2C', '1'))

setup(name='hub',
      version='0.1',
      description='Strawberry library for accessing Shortcake',
      ext_modules=[hub_module])
