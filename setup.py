#! /usr/bin/env python3

from setuptools import setup, Extension

hub_module = Extension('hub',
                       include_dirs = ['include'],
                       sources = ['src/hubmodule.c',
                                  'src/i2c.c',
                                  'src/queue.c',
                                  'src/cmd.c' ])

setup(name='hub',
      version='0.1',
      description='Strawberry library for accessing Shortcake',
      ext_modules=[hub_module])
