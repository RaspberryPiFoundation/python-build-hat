#! /usr/bin/env python3

import random
import resource
import subprocess
import time
import unittest

fake_hat_binary = './test/resources/FakeHat'
if __name__ == "__main__":
        import argparse
        parser = argparse.ArgumentParser(description="Manually test the Python Shortcake Hat library")
        parser.add_argument('fake_hat', default="./test/resources/FakeHat", help="the binary emulating a Shortcake Hat for test purposes")
        args = parser.parse_args()
        fake_hat_binary = args.fake_hat

# Fake Hat for testing purposes
fakeHat = subprocess.Popen(fake_hat_binary, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
time.sleep(0.5) # Sometimes FakeHat taks a little while to initialise
from hub import hub # isort:skip

# Attaching a dummy to port A
fakeHat.stdin.write(b'attach a $dummy\n')
fakeHat.stdin.flush()
fakeHat.stdin.write(b'attach c $motor\n')
fakeHat.stdin.flush()
fakeHat.stdin.write(b'attach d $motor\n')
fakeHat.stdin.flush()
time.sleep(0.1)
time.sleep(1)


# These tests should pass regardless of the state of the hat
class GeneralTestCase(unittest.TestCase):
	def test_hub_type(self):
                # The Shortcake Hat does not have "temperature" or (at
                # least at present) "firmware" attributes, and it's
                # debatable whether it should have the "power_off()"
                # method.
		#expected_values = {'firmware', 'info', 'port', 'power_off', 'status', 'temperature'}
		expected_values = {'info', 'port', 'status'}
		for x in expected_values:
			with self.subTest(msg='Checking that p1 is in dir(hub)', p1=x):
				self.assertIn(x,dir(hub))

	def test_hub_info_type(self):
		assert isinstance(hub.info(), dict)

	def test_hub_info_keys(self):
		assert 'hardware_revision' in hub.info().keys()

	def test_port_types(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
			assert isinstance(P.info(), dict)
			assert {'type'}.issubset(P.info().keys())

	@unittest.skip("Mode not implemented yet")
	def test_port_mode_implemented(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			try:
				P.mode()
			except NotImplementedError:
				self.fail("Mode not implemented yet")

# These tests must be done with a dummy attached to port A
class DummyAttachedATestCase(unittest.TestCase):
	def test_dummy_port_info(self):
		assert isinstance(hub.port.A.info(), dict)
		assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(hub.port.A.info().keys())
		assert isinstance(hub.port.A.info()['modes'], list)
		assert {'name', 'raw', 'pct', 'si', 'symbol', 'map_out', 'map_in', 'capability', 'format'}.issubset(hub.port.A.info()['modes'][1].keys())

	def test_port_device_mode(self):
		assert {'mode'}.issubset(dir(hub.port.A.device))
		try:
			isinstance(hub.port.A.device.mode(), dict)
		except NotImplementedError:
			self.skipTest('Mode not implemented')

# These tests must be done with nothing attached to port F
class PortDetachedBTestCase(unittest.TestCase):
	def test_port_info(self):
		assert isinstance(hub.port.F.info(), dict)
		assert hub.port.F.info() == {'type': None}

	def test_port_device(self):
		assert hub.port.F.device is None

# These tests must be done with a motor attached to port C
class MotorAttachedCTestCase(unittest.TestCase):
	def test_port_B_type_with_motor_connected(self):
		P = hub.port.C
		assert {'callback', 'device', 'info', 'mode', 'motor', 'pwm'}.issubset(dir(P)) 
		assert isinstance(P.info(), dict)
		assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(P.info().keys())

	def test_motor_C_type_with_motor_connected(self):
		expected_values = {'BUSY_MODE', 'BUSY_MOTOR', 'EVENT_COMPLETED', 'EVENT_INTERRUPTED', 'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'PID_POSITION', 'PID_SPEED', 'STOP_BRAKE', 'STOP_FLOAT', 'STOP_HOLD', 'brake', 'busy', 'callback', 'default', 'float', 'get', 'hold', 'mode', 'pair', 'pid', 'preset', 'pwm', 'run_at_speed', 'run_for_degrees', 'run_for_time', 'run_to_position'}
		for x in expected_values:
			with self.subTest(msg='Checking that p1 is in dir(hub.port.C.motor)', p1=x):
				self.assertIn(x,dir(hub.port.C.motor))

	def test_motor_C_default_type(self):
		expected_values = {'speed', 'max_power', 'acceleration', 'deceleration', 'stop', 'pid', 'stall', 'callback'}
		for x in expected_values:
			with self.subTest(msg='Checking that p1 is in hub.port.C.motor.default().keys()', p1=x):
				self.assertIn(x,hub.port.C.motor.default().keys())

	def test_motor_C_basic_functionality_with_motor_connected(self):
		hub.port.C.motor.brake()
		hub.port.C.motor.float()
		hub.port.C.motor.get()

	def test_motor_C_movement_functionality_with_motor_connected(self):
		hub.port.C.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
		hub.port.C.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
		hub.port.C.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
		hub.port.C.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
		hub.port.C.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
		hub.port.C.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
		hub.port.C.motor.pid()

# Motors must be connected to ports C and D
class MotorPairCDTestCase(unittest.TestCase):
	def test_create_motor_pair(self):
		pair = hub.port.C.motor.pair(hub.port.D.motor)
		self.assertEqual(pair.primary(), hub.port.C.motor)
		self.assertEqual(pair.secondary(), hub.port.D.motor)
		pair.unpair()

	def test_motor_pair_functionality(self):
		pair = hub.port.C.motor.pair(hub.port.D.motor)
		pair.brake()
		pair.float()
		pair.run_for_time(1000, 127, 127)
		pair.run_for_time(1000, -127, -127)
		pair.run_at_speed(-100, 50)
		pair.run_to_position(0, 127, 70)


# Touch sensor must be connected to port B
@unittest.skip("Using FakeHat")
class TouchSensorBTestCase(unittest.TestCase):
	def test_touch_sensor_B_type(self):
		assert {'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'get', 'mode', 'pwm'}.issubset(dir(hub.port.B.device))

	def test_touch_sensor_B_functionality(self):
		assert hub.port.B.device.get() <= 1 # Without button pressed
		assert hub.port.B.device.get() >= 0 # At all times
		assert hub.port.B.device.get() <= 9 # At all times

if __name__ == '__main__':
	unittest.main(argv=['first arg is ignored'])
