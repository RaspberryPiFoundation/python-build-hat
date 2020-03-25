import subprocess
import time

fakeHat = subprocess.Popen('test/resources/FakeHat', stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

time.sleep(0.5)

from hub import hub # isort:skip

for i in range(10):
	for i in range(20):
		fakeHat.stdin.write(b'detach a\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		fakeHat.stdin.write(b'attach a $motor\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)


	# hub.port.A.motor.run_to_position(42, 100)

	for i in range(360):
		hub.port.A.motor.run_to_position(i, 100)


# sometimes I get:
#
# Traceback (most recent call last):
#   File "test/test_script_minimal_2.py", line 22, in <module>
#     hub.port.A.motor.run_to_position(i, 100)
# AttributeError: 'NoneType' object has no attribute 'run_to_position'
#
#
# Other times I get:
# Traceback (most recent call last):
#   File "test/test_script_minimal_2.py", line 22, in <module>
#     hub.port.A.motor.run_to_position(i, 100)
# TimeoutError: [Errno 110] Connection timed out
