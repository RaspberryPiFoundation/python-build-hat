#! /usr/bin/env python3

# Copyright (c) 2020 Raspberry Pi (Trading) Limited
#
# SPDX-License-Identifier: MIT

from setuptools import setup, Extension
from os import getenv

LIB_VERSION="0.3.0"

with open("README.md") as readme:
    long_description = readme.read()

hub_module = Extension('build_hat',
                       include_dirs = ['include'],
                       sources = ['src/hubmodule.c',
                                  'src/i2c.c',
                                  'src/queue.c',
                                  'src/cmd.c',
                                  'src/port.c',
                                  'src/device.c',
                                  'src/motor.c',
                                  'src/pair.c',
                                  'src/callback.c',
                                  'src/firmware.c'])
hub_module.define_macros.append(('LIB_VERSION', LIB_VERSION))

# If DEBUG_I2C is set, extra commands are added to the hub
# module to facilite debugging.
if getenv("DEBUG_I2C") == "1":
    hub_module.sources.append('src/debug-i2c.c')
    hub_module.define_macros.append(('DEBUG_I2C', '1'))

setup(name='build_hat',
      version=LIB_VERSION,
      description='Strawberry library for accessing Shortcake',
      long_description=long_description,
      long_description_content_type="text/markdown",
      author='Rhodri James',
      author_email='rhodri@kynesim.co.uk',
      packages=['buildhat'],
      ext_modules=[hub_module])
