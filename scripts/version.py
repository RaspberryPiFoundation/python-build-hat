#! /usr/bin/env python3

from build_hat import BuildHAT

bh = BuildHAT()
info = bh.info()
print("Firmware version:", info["firmware_revision"])
