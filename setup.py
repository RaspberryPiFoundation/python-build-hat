#! /usr/bin/env python3

"""Setup file"""

# Copyright (c) 2020-2025 Raspberry Pi Foundation
#
# SPDX-License-Identifier: MIT

from setuptools import setup

with open("README.md") as readme:
    long_description = readme.read()

with open('VERSION') as versionf:
    version = versionf.read().strip()

setup(name='buildhat',
      version=version,
      description='Build HAT Python library',
      license='MIT',
      long_description=long_description,
      long_description_content_type="text/markdown",
      author='Raspberry Pi Foundation',
      author_email='web@raspberrypi.org',
      url='https://github.com/RaspberryPiFoundation/python-build-hat',
      project_urls={'Bug Tracker': "https://github.com/RaspberryPiFoundation/python-build-hat/issues"},
      packages=['buildhat'],
      package_data={
          "": ["data/firmware.bin", "data/signature.bin", "data/version"],
      },
      python_requires='>=3.7',
      install_requires=['gpiozero', 'pyserial'])
