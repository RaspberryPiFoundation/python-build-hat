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

	assert {'info'}.issubset(dir(hub))

finally:
	fakeHat.terminate()
	fakeHat.kill()
