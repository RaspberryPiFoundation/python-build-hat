#! /usr/bin/env python3

import numbers
import os
import random
import select
import subprocess
import time
import unittest
from unittest import mock

import psutil

process = psutil.Process(os.getpid())

fake_hat_binary = './test/resources/FakeHat'
if __name__ == "__main__":
        import argparse
        parser = argparse.ArgumentParser(description="Manually test the Python Shortcake Hat library")
        parser.add_argument('fake_hat', default="./test/resources/FakeHat", help="the binary emulating a Shortcake Hat for test purposes")
        args = parser.parse_args()
        fake_hat_binary = args.fake_hat

# Fake Hat for testing purposes
fakeHat = subprocess.Popen(fake_hat_binary, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(0.5) # Sometimes FakeHat taks a little while to initialise
from hub import hub # isort:skip
time.sleep(0.5)

fakeHat.stdin.write(b'attach a $motor\n')
fakeHat.stdin.flush()
time.sleep(0.1)

port = hub.port.A

port.device.mode([(2,0),(3,0)])
x = port.device.get()

# everything is fine without these lines
fakeHat.stdin.write(b'detach a\n')
fakeHat.stdin.flush()
time.sleep(0.5)
fakeHat.stdin.write(b'attach a $motor\n')
fakeHat.stdin.flush()

port = hub.port.A

time.sleep(0.1)
port.motor.get() # this is where the timeout occurs

fakeHat.stdin.write(b'detach a\n')
fakeHat.stdin.flush()
