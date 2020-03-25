#! /usr/bin/env python3

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

def defaultsetup():
	time.sleep(0.1)
	fakeHat.stdin.write(b'attach a $dummy\n')
	fakeHat.stdin.write(b'attach c $motor\n')
	fakeHat.stdin.write(b'attach d $motor\n')
	fakeHat.stdin.flush()
	time.sleep(0.1)
	
def detachall():
	time.sleep(0.1)
	fakeHat.stdin.write(b'detach a\n')
	fakeHat.stdin.write(b'detach b\n')
	fakeHat.stdin.write(b'detach c\n')
	fakeHat.stdin.write(b'detach d\n')
	fakeHat.stdin.write(b'detach e\n')
	fakeHat.stdin.write(b'detach f\n')
	fakeHat.stdin.flush()
	time.sleep(0.1)
	

class portAttachDetachTestCase(unittest.TestCase):
	def test_repeated_attach_detach(self):
		for i in range(20):
			fakeHat.stdin.write(b'detach a\n')
			fakeHat.stdin.write(b'attach a $motor\n')
		fakeHat.stdin.flush()
		time.sleep(2)
		for i in range(360):
			hub.port.A.motor.run_to_position(i, 100)

# These tests should pass regardless of the state of the hat
class GeneralTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()


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
		self.assertIsInstance(hub.info(), dict)

	def test_hub_info_keys_hw_version(self):
		self.assertIn('hw_version',hub.info().keys())

	def test_hub_info_keys_fw_version(self):
		self.assertIn('fw_version',hub.info().keys())

	def test_hub_status_type(self):
		self.assertIsInstance(hub.info(), dict)

	def test_hub_status_keys(self):
		assert 'port' in hub.status().keys()
		assert len(hub.status())==1

	def test_hub_status_port_keys(self):
		assert {'A','B','C','D','E','F'}.issubset(hub.status()['port'].keys())

	def test_port_types(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.E, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			with self.subTest(port = P):
				assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
				self.assertIsInstance(P.info(), dict)
				for key in {'type'}:
					with self.subTest(key=key):
						self.assertIn(key,P.info().keys())

	@unittest.skip("Using Port mode not implemented")
	def test_port_mode_implemented(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			try:
				P.mode()
			except NotImplementedError:
				self.fail("Mode not implemented yet")

class MixedAttachedTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_dummy_port_types(self):
		ports = [hub.port.A]
		random.shuffle(ports)
		for P in ports:
			with self.subTest(port = P):
				assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
				self.assertIsInstance(P.info(), dict)
				for key in {'type','fw_version','hw_version','combi_modes','modes'}:
					with self.subTest(key=key):
						self.assertIn(key,P.info().keys())
				self.assertIsInstance(P.info()['modes'],list)
				self.assertIsInstance(P.device.get(0),list)
				self.assertIsInstance(P.device.get(1),list)
				self.assertIsInstance(P.device.get(2),list)
				for M in P.info()['modes']:
					assert {'name','capability','symbol','raw','pct','si','map_out','map_in','format'}.issubset(M.keys())
					assert {'datasets','figures','decimals','type'}.issubset(M['format'])
					assert 0 <= M['format']['type'] <= 3
					assert 0 <= M['map_out'] <= 255
					assert 0 <= M['map_in'] <= 255

	def test_motor_port_types(self):
		ports = [hub.port.C, hub.port.D]
		random.shuffle(ports)
		for P in ports:
			with self.subTest(port = P):
				assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
				self.assertIsInstance(P.info(), dict)
				for key in {'type','fw_version','hw_version','combi_modes','modes'}:
					with self.subTest(key=key):
						self.assertIn(key,P.info().keys())
				self.assertIsInstance(P.info()['modes'],list)
				self.assertIsInstance(P.device.get(0),list)
				self.assertIsInstance(P.device.get(1),list)
				self.assertIsInstance(P.device.get(2),list)
				for M in P.info()['modes']:
					assert {'name','capability','symbol','raw','pct','si','map_out','map_in','format'}.issubset(M.keys())
					assert {'datasets','figures','decimals','type'}.issubset(M['format'])
					assert 0 <= M['format']['type'] <= 3
					assert 0 <= M['map_out'] <= 255
					assert 0 <= M['map_in'] <= 255

	def test_empty_types(self):
		ports = [hub.port.B, hub.port.E, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			with self.subTest(port = P):
				assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
				self.assertIsInstance(P.info(), dict)
				self.assertEqual(P.info().keys(),{'type'})
				self.assertIsNone(P.device)

# These tests must be done with a dummy attached to port A
class DummyAttachedATestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_dummy_port_info(self):
		self.assertIsInstance(hub.port.A.info(), dict)
		assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(hub.port.A.info().keys())
		self.assertIsInstance(hub.port.A.info()['modes'], list)
		assert {'name', 'raw', 'pct', 'si', 'symbol', 'map_out', 'map_in', 'capability', 'format'}.issubset(hub.port.A.info()['modes'][1].keys())

	def test_port_device_mode_read(self):
		assert {'mode'}.issubset(dir(hub.port.A.device))
		try:
			self.assertIsInstance(hub.port.A.device.mode(), list)
			self.assertIsInstance(hub.port.A.device.mode()[0], tuple)
			self.assertEqual(len(hub.port.A.device.mode()[0]), 2)
		except NotImplementedError:
			self.fail('Mode not implemented')

	def test_port_device_mode_set(self):
		assert {'mode'}.issubset(dir(hub.port.A.device))
		try:
			self.assertIsInstance(hub.port.A.device.mode(), list)
			self.assertIsInstance(hub.port.A.device.mode()[0], tuple)
			self.assertEqual(len(hub.port.A.device.mode()[0]), 2)
			hub.port.A.device.mode(1)
			self.assertIsInstance(hub.port.A.device.mode(), list)
			self.assertIsInstance(hub.port.A.device.mode()[0], tuple)
			self.assertEqual(len(hub.port.A.device.mode()[0]), 2)
			hub.port.A.device.mode([(3,4),(5,6)])
			self.assertIsInstance(hub.port.A.device.mode(), list)
			self.assertIsInstance(hub.port.A.mode()[0], tuple)
			self.assertEqual(len(hub.port.A.device.mode()[0]), 2)
		except NotImplementedError:
			self.fail('Mode not implemented')

# These tests must be done with nothing attached to port F
class PortDetachedFTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_port_info(self):
		self.assertIsInstance(hub.port.F.info(), dict)
		assert hub.port.F.info() == {'type': None}

	def test_port_device(self):
		assert hub.port.F.device is None

# These tests must be done with a dummy attached to port A
class PWMATestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_pwm_values(self):
		hub.port.A.pwm(100)
		hub.port.A.pwm(-100)
		with self.assertRaises(ValueError):
			hub.port.A.pwm(101)
		with self.assertRaises(ValueError):
			hub.port.A.pwm(-101)
		hub.port.A.pwm(0)

# These tests must be done with a motor attached to port C
class MotorAttachedCTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_port_B_type_with_motor_connected(self):
		P = hub.port.C
		assert {'callback', 'device', 'info', 'mode', 'motor', 'pwm'}.issubset(dir(P)) 
		self.assertIsInstance(P.info(), dict)
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
		hub.port.C.motor.preset(0)
		hub.port.C.motor.hold(100)
		hub.port.C.motor.hold(0)
		hub.port.C.motor.brake()
		hub.port.C.motor.float()
		hub.port.C.motor.get()
		self.assertIsInstance(hub.port.C.motor.busy(0),bool)
		self.assertIsInstance(hub.port.C.motor.busy(1),bool)
		self.assertIsInstance(hub.port.C.motor.default(), dict)
		assert {'speed','max_power','acceleration','deceleration','stall','callback','stop','pid'}.issubset(hub.port.C.motor.default().keys())

	def test_motor_C_movement_functionality_with_motor_connected(self):
		hub.port.C.motor.run_at_speed(10, 100,10000,10000)
		hub.port.C.motor.float()
		hub.port.C.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
		hub.port.C.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
		hub.port.C.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
		hub.port.C.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
		hub.port.C.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
		hub.port.C.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
		self.assertIsInstance(hub.port.C.motor.pid(),tuple)
		assert len(hub.port.C.motor.pid()) == 3

	def test_motor_callbacks(self):
		hub.port.C.motor.callback(None)

		mymock = mock.Mock()
		mymock.method.assert_not_called()
		assert hub.port.C.motor.callback() is None
		hub.port.C.motor.callback(mymock.method)
		assert callable(hub.port.C.motor.callback())
		mymock.method.assert_not_called()
		hub.port.C.motor.brake()
		mymock.method.assert_called_once()

		hub.port.C.motor.callback(None)

	def test_motor_callbacks_old(self):
		hub.port.C.motor.callback(None)

		global myflag
		myflag = 0
		def myfun(x):
			global myflag
			myflag=1

		assert hub.port.C.motor.callback() is None
		hub.port.C.motor.callback(myfun)
		assert callable(hub.port.C.motor.callback())
		assert myflag==0
		hub.port.C.motor.brake()
		assert myflag==1

		hub.port.C.motor.callback(None)



# Motors must be connected to ports C and D
class MotorPairCDTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_create_motor_pair(self):
		pair = hub.port.C.motor.pair(hub.port.D.motor)
		self.assertEqual(pair.primary(), hub.port.C.motor)
		self.assertEqual(pair.secondary(), hub.port.D.motor)
		pair.unpair()

	def test_motor_pair_functionality(self):
		pair = hub.port.C.motor.pair(hub.port.D.motor)
		pair.preset(0,0)
		pair.hold(100)
		pair.hold(0)
		pair.brake()
		pair.float()
		assert pair.pid() == (0,0,0)
		self.assertIsInstance(pair.pid(),tuple)
		assert len(pair.pid()) == 3
		pair.run_at_speed(10,10, 100,10000,10000)
		pair.run_for_degrees(180, 127,127)
		pair.run_for_time(1000, 127, 127)
		pair.run_for_time(1000, -127, -127)
		pair.run_to_position(0, 127, 70)
		pair.unpair()

# Motor must be connected to port C
class MemoryLeakTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def testMemoryLeaks(self):
		startmemory = process.memory_info().rss
		hub.port.C.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
		hub.port.C.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
		hub.port.C.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
		hub.port.C.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
		hub.port.C.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
		hub.port.C.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
		hub.port.C.motor.pid()
		hub.port.C.motor.brake()
		hub.port.C.motor.float()
		hub.port.C.motor.get()
		pair = hub.port.C.motor.pair(hub.port.D.motor)
		pair.brake()
		pair.float()
		pair.run_for_time(1000, 127, 127)
		pair.run_for_time(1000, -127, -127)
		pair.run_at_speed(-100, 50)
		pair.run_to_position(0, 127, 70)
		pair.unpair()
		onerunmemory = process.memory_info().rss
		hub.port.C.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
		hub.port.C.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
		hub.port.C.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
		hub.port.C.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
		hub.port.C.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
		hub.port.C.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
		hub.port.C.motor.pid()
		hub.port.C.motor.brake()
		hub.port.C.motor.float()
		hub.port.C.motor.get()
		pair = hub.port.C.motor.pair(hub.port.D.motor)
		pair.brake()
		pair.float()
		pair.run_for_time(1000, 127, 127)
		pair.run_for_time(1000, -127, -127)
		pair.run_at_speed(-100, 50)
		pair.run_to_position(0, 127, 70)
		pair.unpair()
		tworunmemory = process.memory_info().rss
		fakeHat.stdin.write(b'detach a\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		fakeHat.stdin.write(b'attach a $dummy\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		fakeHat.stdin.write(b'detach a\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		fakeHat.stdin.write(b'attach a $dummy\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		self.assertLessEqual(tworunmemory, onerunmemory)


# Touch sensor must be connected to port B
@unittest.skip("Using FakeHat")
class TouchSensorBTestCase(unittest.TestCase):
	def setUp(self):
		defaultsetup()

	def tearDown(self):
		detachall()

	def test_touch_sensor_B_type(self):
		assert {'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'get', 'mode', 'pwm'}.issubset(dir(hub.port.B.device))

	def test_touch_sensor_B_functionality(self):
		assert hub.port.B.device.get() <= 1 # Without button pressed
		assert hub.port.B.device.get() >= 0 # At all times
		assert hub.port.B.device.get() <= 9 # At all times

class PortCallbackATestCase(unittest.TestCase):
	def tearDown(self):
		detachall()

	def test_connect_disconnect_port_callbacks(self):
		mymock = mock.Mock()
		mymock.method.assert_not_called()
		hub.port.A.callback(mymock.method)
		mymock.method.assert_not_called()
		mymock.method()
		self.assertEqual(mymock.method.call_count, 1)
		fakeHat.stdin.write(b'attach a $dummy\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		self.assertEqual(mymock.method.call_count, 2)
		mymock.method.assert_called_with(hub.port.ATTACHED)
		fakeHat.stdin.write(b'detach a\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		self.assertEqual(mymock.method.call_count, 3)
		mymock.method.assert_called_with(hub.port.DETACHED)

	def test_connect_disconnect_port_callbacks_old(self):
		global myflag
		myflag = 0
		def myfun(x):
			global myflag
			myflag=1

		self.assertEqual(myflag, 0)
		myfun(1)
		self.assertEqual(myflag, 1)
		myflag = 0

		hub.port.A.callback(myfun)

		self.assertEqual(myflag, 0)
		fakeHat.stdin.write(b'attach a $dummy\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		self.assertEqual(myflag, 1)
		myflag = 0

		self.assertEqual(myflag, 0)
		fakeHat.stdin.write(b'detach a\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)
		self.assertEqual(myflag, 1)
		myflag = 0


if __name__ == '__main__':
	unittest.main(argv=['first arg is ignored'])
