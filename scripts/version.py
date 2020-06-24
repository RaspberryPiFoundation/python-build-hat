#! /usr/bin/env python3

from hub import hub

info = hub.info()
print("Firmware version:", info["firmware_revision"])
