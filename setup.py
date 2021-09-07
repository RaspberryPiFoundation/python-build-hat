#! /usr/bin/env python3

# Copyright (c) 2020-2021 Raspberry Pi Foundation
#
# SPDX-License-Identifier: MIT

from setuptools import setup, Extension
from os import getenv

with open("README.md") as readme:
    long_description = readme.read()

with open('VERSION') as versionf:
    version = versionf.read().strip()

setup(name='buildhat',
      version=version,
      description='Build HAT Python library',
      long_description=long_description,
      long_description_content_type="text/markdown",
      author='Raspberry Pi Foundation',
      author_email = 'web@raspberrypi.org',
      url = 'https://github.com/RaspberryPiFoundation/buildhat',
      project_urls = {
        'Bug Tracker': "https://github.com/RaspberryPiFoundation/buildhat/issues",
      },
      packages=['buildhat'],
      package_data={
          "": ["data/firmware.bin", "data/signature.bin", "data/version"],
      },
      python_requires = '>=3.8',
      install_requires=['gpiozero','pyserial']),
