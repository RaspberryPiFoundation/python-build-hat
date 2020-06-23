#! /usr/bin/env python3

"""
This program writes the upgrade software for a Raspberry Pi Build HAT.
Invoke it as:

    $ upgrade_hat.py <firmware.bin>
"""

import hub
import sys
from time import sleep

firmware = hub.hub.firmware

erase_complete_flag = 0

def erase_callback(reason, status):
    global erase_complete_flag
    if status == 0:
        erase_complete_flag = -1
    else:
        erase_complete_flag = 1


# Get the binary file to download
if len(sys.argv) != 2:
    print("Syntax: upgrade_hat.py <filename>")
    sys.exit(2)

with open(sys.argv[1], "rb") as firmware_file:
    firmware_bytes = firmware_file.read()


firmware.callback(erase_callback)
firmware.appl_image_initialize(len(firmware_bytes))

# It will take some time (~9 seconds) for initialisation to complete

sys.stdout.write("Erasing")
sys.stdout.flush()
while erase_complete_flag == 0:
    sys.stdout.write(".")
    sys.stdout.flush()
    sleep(1)

if erase_complete_flag < 0:
    sys.exit("Error: erase failed")
print("Done")

bytes_written = 0
bytes_remaining = len(firmware_bytes)

while bytes_remaining >= 1024:
    print("\rWriting {}%".format(100*bytes_written // len(firmware_bytes)), end="")
    sys.stdout.flush()
    firmware.appl_image_store(firmware_bytes[bytes_written:bytes_written+1024])
    bytes_written += 1024
    bytes_remaining -= 1024

if bytes_remaining != 0:
    print("\rWriting {}%".format(100*bytes_written // len(firmware_bytes)), end="")
    sys.stdout.flush()
    firmware.appl_image_store(firmware_bytes[bytes_written:])

print("\rWriting 100%")
print("Checking...")
info = firmware.info()

if not info["new_appl_valid"]:
    print("***New application invalid, will not be used")
    print("Stored checksum: 0x{:08x}".
          format(info["new_appl_image_stored_checksum"]))
    print("Calculated checksum: 0x{:08x}".
          format(info["new_appl_image_calc_checksum"]))
    sys.exit(1)

print("Done")
