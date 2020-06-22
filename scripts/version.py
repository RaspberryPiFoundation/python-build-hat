#! /usr/bin/env python3

from hub import hub
from time import sleep

info = hub.info()

sleep(2)
print("Firmware version:", info["firmware_revision"])
