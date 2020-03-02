#! /usr/bin/env python3

import random
import subprocess
import time

fake_hat_binary = '/home/max//kynesim/322/FakeHat/bin/FakeHat'
if __name__ == "__main__":
        import argparse
        parser = argparse.ArgumentParser(description="Manually test the Python Shortcake Hat library")
        parser.add_argument('fake_hat', default="/home/max/kynesim/322/FakeHat/bin/FakeHat", help="the binary emulating a Shortcake Hat for test purposes")
        args = parser.parse_args()
        fake_hat_binary = args.fake_hat

try:
# Fake Hat for testing purposes
	fakeHat = subprocess.Popen(fake_hat_binary, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	time.sleep(0.5) # Sometimes FakeHat taks a little while to initialise
	import hub # isort:skip
	# Attaching a dummy to port A
	fakeHat.stdin.write(b'attach a $dummy\n')
	fakeHat.stdin.flush()
	time.sleep(1)

	assert isinstance(hub.info(), dict)

	assert {'hardware_revision'}.issubset(hub.info().keys()) # From real hub


	ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
	random.shuffle(ports)
	for P in ports:
		assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
		assert isinstance(P.info(), dict)
		assert {'type'}.issubset(P.info().keys())


	# These tests must be done with a dummy attached to port A
	assert isinstance(hub.port.A.info(), dict)
	assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(hub.port.A.info().keys())
	assert isinstance(hub.port.A.info()['modes'], list)
	assert {'name', 'raw', 'pct', 'si', 'symbol', 'map_out', 'map_in', 'capability', 'format'}.issubset(hub.port.A.info()['modes'][1].keys())

	assert {'mode'}.issubset(dir(hub.port.A.device))
	try:
		isinstance(hub.port.A.device.mode(), dict)
	except NotImplementedError:
		self.skipTest('Mode not implemented')

	# These tests must be done with nothing attached to port F
	assert isinstance(hub.port.F.info(), dict)
	assert hub.port.F.info() == {'type': None}

	assert hub.port.F.device is None

finally:
	fakeHat.terminate()
	fakeHat.kill()
