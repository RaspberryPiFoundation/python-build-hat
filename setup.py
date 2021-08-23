#! /usr/bin/env python3

# Copyright (c) 2020 Raspberry Pi (Trading) Limited
#
# SPDX-License-Identifier: MIT

from setuptools import setup, Extension
from os import getenv

LIB_VERSION="0.4.5"

with open("README.md") as readme:
    long_description = readme.read()

setup(name='build_hat',
      version=LIB_VERSION,
      description='Strawberry library for accessing Shortcake',
      long_description=long_description,
      long_description_content_type="text/markdown",
      author='Rhodri James',
      author_email='rhodri@kynesim.co.uk',
      packages=['buildhat'],
      package_data={
          "": ["data/firmware.bin", "data/signature.bin", "data/version"],
      },
      install_requires=['gpiozero','pyserial'])
