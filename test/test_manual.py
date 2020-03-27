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
		self.ports = {hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.E, hub.port.F}

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
		self.assertIn('hardware_revision',hub.info().keys())

	def test_hub_info_keys_fw_version(self):
		self.assertIn('firmware_revision',hub.info().keys())

	def test_hub_status_type(self):
		self.assertIsInstance(hub.info(), dict)

	def test_hub_status_keys(self):
		assert 'port' in hub.status().keys()
		assert len(hub.status())==1

	def test_hub_status_port_keys(self):
		assert {'A','B','C','D','E','F'}.issubset(hub.status()['port'].keys())

	def test_port_types(self):
		for P in self.ports:
			with self.subTest(port = P):
				assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
				self.assertIsInstance(P.info(), dict)
				for key in {'type'}:
					with self.subTest(key=key):
						self.assertIn(key,P.info().keys())
				self.assertEqual(hub.port.ATTACHED,1)
				self.assertEqual(hub.port.DETACHED,0)

	@unittest.skip("Using Port mode not implemented")
	def test_port_mode_implemented(self):
		for P in self.ports:
			with self.subTest(port=P):
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
				self.assertIsNone(P.motor)

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
				self.assertIsNone(P.motor)

	def test_port_device_get(self):
		devices = {hub.port.A.device, hub.port.C.device, hub.port.D.device}
		for device in devices:
			with self.subTest(device = device):
				self.assertEqual(device.FORMAT_RAW,0)
				self.assertEqual(device.FORMAT_PCT,1)
				self.assertEqual(device.FORMAT_SI,2)
				for get_format in {device.FORMAT_RAW, device.FORMAT_PCT, device.FORMAT_SI}:
					with self.subTest(get_format = get_format):
						self.assertIsInstance(device.get(get_format), list)
						if len(device.get(get_format))>0:
							self.assertIsInstance(device.get(get_format)[0], numbers.Real) # Apparently python thinks that floats are irrational


class DummyTestCase(unittest.TestCase):

	def setUp(self):
		time.sleep(0.1)
		fakeHat.stdin.write(b'attach a $dummy\n')
		fakeHat.stdin.write(b'attach b $dummy\n')
		fakeHat.stdin.write(b'attach c $dummy\n')
		fakeHat.stdin.write(b'attach d $dummy\n')
		fakeHat.stdin.write(b'attach e $dummy\n')
		fakeHat.stdin.write(b'attach f $dummy\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)

		self.ports = {hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.E, hub.port.F}

	def tearDown(self):
		detachall()

	def test_dummy_port_info(self):
		for port in self.ports:
			with self.subTest(port=port):
				self.assertIsInstance(port.info(), dict)
				assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(port.info().keys())
				self.assertIsInstance(port.info()['modes'], list)
				assert {'name', 'raw', 'pct', 'si', 'symbol', 'map_out', 'map_in', 'capability', 'format'}.issubset(port.info()['modes'][1].keys())

	def test_port_device_mode_read(self):
		for port in self.ports:
			with self.subTest(port=port):
				assert {'mode'}.issubset(dir(port.device))
				try:
					self.assertIsInstance(port.device.mode(), list)
					self.assertIsInstance(port.device.mode()[0], tuple)
					self.assertEqual(len(port.device.mode()[0]), 2)
				except NotImplementedError:
					self.fail('Mode not implemented')

	def test_port_device_mode_set(self):
		for port in self.ports:
			with self.subTest(port=port):
				assert {'mode'}.issubset(dir(port.device))
				try:
					self.assertIsInstance(port.device.mode(), list)
					self.assertIsInstance(port.device.mode()[0], tuple)
					self.assertEqual(len(port.device.mode()[0]), 2)
					port.device.mode(1)
					self.assertIsInstance(port.device.mode(), list)
					self.assertIsInstance(port.device.mode()[0], tuple)
					self.assertEqual(len(port.device.mode()[0]), 2)
					port.device.mode([(3,4),(5,6)])
					self.assertIsInstance(port.device.mode(), list)
					self.assertIsInstance(port.mode()[0], tuple)
					self.assertEqual(len(port.device.mode()[0]), 2)
				except NotImplementedError:
					self.fail('Mode not implemented')

	def test_pwm_values(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.pwm(100)
				port.pwm(-100)
				with self.assertRaises(ValueError):
					port.pwm(101)
				with self.assertRaises(ValueError):
					port.pwm(-101)
				port.pwm(0)

	def test_combi_modes(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.device.mode([(0,0),(1,0),(1,2),(3,0)])
				x = port.device.get()
				self.assertIsInstance(x, list)
				self.assertIsInstance(x[0],tuple)
				self.assertEqual(len(x), 4)
				self.assertEqual(len(x[0]),2)

class PortDetachedTestCase(unittest.TestCase):
	def setUp(self):
		detachall()
		self.ports = {hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.E, hub.port.F}

	def tearDown(self):
		detachall()

	def test_port_info(self):
		for port in self.ports:
			with self.subTest(port=port):
				self.assertIsInstance(port.info(), dict)
				assert port.info() == {'type': None}

	def test_port_device(self):
		for port in self.ports:
			with self.subTest(port=port):
				assert port.device is None


class MotorTestCase(unittest.TestCase):
	def setUp(self):
		time.sleep(0.1)
		fakeHat.stdin.write(b'attach a $motor\n')
		fakeHat.stdin.write(b'attach b $motor\n')
		fakeHat.stdin.write(b'attach c $motor\n')
		fakeHat.stdin.write(b'attach d $motor\n')
		fakeHat.stdin.write(b'attach e $motor\n')
		fakeHat.stdin.write(b'attach f $motor\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)

		self.ports = {hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.E, hub.port.F}

	def tearDown(self):
		detachall()

	def test_port_type_with_motor_connected(self):
		for port in self.ports:
			with self.subTest(port=port):
				assert {'callback', 'device', 'info', 'mode', 'motor', 'pwm'}.issubset(dir(port)) 
				self.assertIsInstance(port.info(), dict)
				assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(port.info().keys())

	def test_motor_type_with_motor_connected(self):
		for port in self.ports:
			with self.subTest(port=port):
				expected_values = {'BUSY_MODE', 'BUSY_MOTOR', 'EVENT_COMPLETED', 'EVENT_INTERRUPTED', 'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'PID_POSITION', 'PID_SPEED', 'STOP_BRAKE', 'STOP_FLOAT', 'STOP_HOLD', 'brake', 'busy', 'callback', 'default', 'float', 'get', 'hold', 'mode', 'pair', 'pid', 'preset', 'pwm', 'run_at_speed', 'run_for_degrees', 'run_for_time', 'run_to_position'}
				for x in expected_values:
					with self.subTest(msg='Checking that p1 is in dir(port.motor)', p1=x):
						self.assertIn(x,dir(port.motor))

	def test_motor_default_type(self):
		for port in self.ports:
			with self.subTest(port=port):
				expected_values = {'speed', 'max_power', 'acceleration', 'deceleration', 'stop', 'pid', 'stall', 'callback'}
				for x in expected_values:
					with self.subTest(msg='Checking that p1 is in port.motor.default().keys()', p1=x):
						self.assertIn(x,port.motor.default().keys())

	def test_combi_modes(self):
		for port in self.ports:
			with self.subTest(port=port):
				self.assertIsInstance(port.info()["combi_modes"], tuple)
				self.assertEqual(port.info()["combi_modes"], (14,15))
				port.device.mode([(2,0),(3,0)])
				x = port.device.get()
				self.assertIsInstance(x, list)
				self.assertIsInstance(x[0],tuple)
				self.assertEqual(len(x), 4)
				self.assertEqual(len(x[0]),2)

	def test_motor_constants(self):
		for port in self.ports:
			with self.subTest(port=port):
				self.assertEqual(port.motor.BUSY_MODE,0)
				self.assertEqual(port.motor.BUSY_MOTOR,1)
				self.assertEqual(port.motor.EVENT_COMPLETED,0)
				self.assertEqual(port.motor.EVENT_INTERRUPTED,1)
				self.assertEqual(port.motor.EVENT_STALL,2)
				self.assertEqual(port.motor.FORMAT_RAW,0)
				self.assertEqual(port.motor.FORMAT_PCT,1)
				self.assertEqual(port.motor.FORMAT_SI,2)
				self.assertEqual(port.motor.PID_SPEED,0)
				self.assertEqual(port.motor.PID_POSITION,1)
				self.assertEqual(port.motor.STOP_FLOAT,0)
				self.assertEqual(port.motor.STOP_BRAKE,1)
				self.assertEqual(port.motor.STOP_HOLD,2)

	def test_motor_get(self):
		for port in self.ports:
			with self.subTest(port=port):
				motor = port.motor
				self.assertEqual(motor.FORMAT_RAW,0)
				self.assertEqual(motor.FORMAT_PCT,1)
				self.assertEqual(motor.FORMAT_SI,2)
				for get_format in {motor.FORMAT_RAW, motor.FORMAT_PCT, motor.FORMAT_SI}:
					with self.subTest(get_format = get_format):
						self.assertIsInstance(motor.get(get_format), list)
						if len(motor.get(get_format))>0:
							self.assertIsInstance(motor.get(get_format)[0], numbers.Real) # Apparently python thinks that floats are irrational


	def test_motor_basic_functionality_with_motor_connected(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.preset(0)
				port.motor.hold(100)
				port.motor.hold(0)
				port.motor.brake()
				port.motor.float()
				port.motor.get()
				self.assertIsInstance(port.motor.busy(0),bool)
				self.assertIsInstance(port.motor.busy(1),bool)
				self.assertIsInstance(port.motor.default(), dict)
				assert {'speed','max_power','acceleration','deceleration','stall','callback','stop','pid'}.issubset(port.motor.default().keys())

	def test_motor_movement_functionality_with_motor_connected(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.run_at_speed(10, 100,10000,10000)
				port.motor.float()
				port.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
				port.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
				port.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
				port.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
				port.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
				port.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
				self.assertIsInstance(port.motor.pid(),tuple)
				assert len(port.motor.pid()) == 3

	# This test fails atm, maybe FakeHat's motor state machine completes commands instantly?
	@unittest.skip('Does not work with FakeHat')
	def test_motor_busy(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.run_for_time(1000, 127)
				self.assertEqual(port.motor.busy(port.motor.BUSY_MOTOR),True)

	def test_motor_callbacks(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.callback(None)

				mymock = mock.Mock()
				mymock.method.assert_not_called()
				assert port.motor.callback() is None
				port.motor.callback(mymock.method)
				assert callable(port.motor.callback())
				mymock.method.assert_not_called()
				port.motor.brake()
				mymock.method.assert_called_once()

				port.motor.callback(None)

	def test_motor_callbacks_old(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.callback(None)

				global myflag
				myflag = 0
				def myfun(x):
					global myflag
					myflag=1

				assert port.motor.callback() is None
				port.motor.callback(myfun)
				assert callable(port.motor.callback())
				assert myflag==0
				port.motor.brake()
				assert myflag==1

				port.motor.callback(None)

	def test_motor_default_callbacks(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.callback(None)

				mymock = mock.Mock()
				mymock.method.assert_not_called()
				port.motor.default(callback=mymock.method)
				mymock.method.assert_not_called()
				port.motor.brake()
				mymock.method.assert_called_once()

				port.motor.callback(None)

	def test_pwm_values(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.pwm(100)
				port.pwm(-100)
				with self.assertRaises(ValueError):
					port.pwm(101)
				with self.assertRaises(ValueError):
					port.pwm(-101)
				port.pwm(0)


# real physical motors need to be attached to all ports for this
@unittest.skip("Using FakeHat")
class PhysicalMotorTestCase(unittest.TestCase):
	def setUp(self):
		time.sleep(0.1)
		fakeHat.stdin.write(b'attach a $motor\n')
		fakeHat.stdin.write(b'attach b $motor\n')
		fakeHat.stdin.write(b'attach c $motor\n')
		fakeHat.stdin.write(b'attach d $motor\n')
		fakeHat.stdin.write(b'attach e $motor\n')
		fakeHat.stdin.write(b'attach f $motor\n')
		fakeHat.stdin.flush()
		time.sleep(0.1)

		self.ports = {hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.E, hub.port.F}

	def tearDown(self):
		detachall()

	def test_port_type_with_motor_connected(self):
		for port in self.ports:
			with self.subTest(port=port):
				assert {'callback', 'device', 'info', 'mode', 'motor', 'pwm'}.issubset(dir(port)) 
				self.assertIsInstance(port.info(), dict)
				assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(port.info().keys())

	def test_motor_type_with_motor_connected(self):
		for port in self.ports:
			with self.subTest(port=port):
				expected_values = {'BUSY_MODE', 'BUSY_MOTOR', 'EVENT_COMPLETED', 'EVENT_INTERRUPTED', 'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'PID_POSITION', 'PID_SPEED', 'STOP_BRAKE', 'STOP_FLOAT', 'STOP_HOLD', 'brake', 'busy', 'callback', 'default', 'float', 'get', 'hold', 'mode', 'pair', 'pid', 'preset', 'pwm', 'run_at_speed', 'run_for_degrees', 'run_for_time', 'run_to_position'}
				for x in expected_values:
					with self.subTest(msg='Checking that p1 is in dir(port.motor)', p1=x):
						self.assertIn(x,dir(port.motor))

	def test_motor_default_type(self):
		for port in self.ports:
			with self.subTest(port=port):
				expected_values = {'speed', 'max_power', 'acceleration', 'deceleration', 'stop', 'pid', 'stall', 'callback'}
				for x in expected_values:
					with self.subTest(msg='Checking that p1 is in port.motor.default().keys()', p1=x):
						self.assertIn(x,port.motor.default().keys())

	def test_motor_constants(self):
		for port in self.ports:
			with self.subTest(port=port):
				self.assertEqual(port.motor.BUSY_MODE,0)
				self.assertEqual(port.motor.BUSY_MOTOR,1)
				self.assertEqual(port.motor.EVENT_COMPLETED,0)
				self.assertEqual(port.motor.EVENT_INTERRUPTED,1)
				self.assertEqual(port.motor.EVENT_STALL,2)
				self.assertEqual(port.motor.FORMAT_RAW,0)
				self.assertEqual(port.motor.FORMAT_PCT,1)
				self.assertEqual(port.motor.FORMAT_SI,2)
				self.assertEqual(port.motor.PID_SPEED,0)
				self.assertEqual(port.motor.PID_POSITION,1)
				self.assertEqual(port.motor.STOP_FLOAT,0)
				self.assertEqual(port.motor.STOP_BRAKE,1)
				self.assertEqual(port.motor.STOP_HOLD,2)

	def test_motor_get(self):
		for port in self.ports:
			with self.subTest(port=port):
				motor = port.motor
				self.assertEqual(motor.FORMAT_RAW,0)
				self.assertEqual(motor.FORMAT_PCT,1)
				self.assertEqual(motor.FORMAT_SI,2)
				for get_format in {motor.FORMAT_RAW, motor.FORMAT_PCT, motor.FORMAT_SI}:
					with self.subTest(get_format = get_format):
						self.assertIsInstance(motor.get(get_format), list)
						if len(motor.get(get_format))>0:
							self.assertIsInstance(motor.get(get_format)[0], numbers.Real) # Apparently python thinks that floats are irrational


	def test_motor_basic_functionality(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.preset(0)
				port.motor.hold(100)
				port.motor.hold(0)
				port.motor.brake()
				port.motor.float()
				port.motor.get()
				self.assertIsInstance(port.motor.busy(0),bool)
				self.assertIsInstance(port.motor.busy(1),bool)
				self.assertIsInstance(port.motor.default(), dict)
				assert {'speed','max_power','acceleration','deceleration','stall','callback','stop','pid'}.issubset(port.motor.default().keys())

	def test_motor_movement_functionality(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.run_to_position(0, 100)
				port.motor.run_at_speed(10, 100,10000,10000)
				port.motor.float()
				port.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
				port.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
				port.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
				port.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
				port.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
				port.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed
				self.assertIsInstance(port.motor.pid(),tuple)
				assert len(port.motor.pid()) == 3

	def test_motor_run_at_speed(self):
		for port in self.ports:
			with self.subTest(port=port):
				mymock.method = mock.Mock()
				mockcalls = mymock.method.call_count
				port.motor.callback(mymock.method)
				port.device.mode(3)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)
				port.motor.run_at_speed(10, 100,10000,10000)
				device.mode(1)
				self.assertNotEqual(port.device.get(),0)
				device.mode(3)
				time.sleep(0.5)
				port.float()
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertNotEqual(port.device.get(),0)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)

	def test_motor_run_for_time(self):
		for port in self.ports:
			with self.subTest(port=port):
				mymock.method = mock.Mock()
				mockcalls = mymock.method.call_count
				port.motor.callback(mymock.method)
				port.device.mode(3)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)
				port.motor.run_for_time(1000, 127)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertNotEqual(port.device.get(),0)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)

	def test_motor_run_for_time2(self):
		for port in self.ports:
			with self.subTest(port=port):
				mymock.method = mock.Mock()
				mockcalls = mymock.method.call_count
				port.motor.callback(mymock.method)
				port.device.mode(3)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)
				port.motor.run_for_time(1000, -50)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertNotEqual(port.device.get(),0)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)

	def test_motor_run_for_degrees(self):
		for port in self.ports:
			with self.subTest(port=port):
				mymock.method = mock.Mock()
				mockcalls = mymock.method.call_count
				port.motor.callback(mymock.method)
				port.device.mode(3)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)
				port.motor.run_for_degrees(180, 99)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertNotEqual(port.device.get(),0)
				port.motor.run_to_position(0, 100)
				while mockcalls == mymock.method.call_count:
					time.sleep(0.1)
				mockcalls = mymock.method.call_count
				self.assertEqual(port.device.get(),0)


	# This test fails atm, maybe FakeHat's motor state machine completes commands instantly?
	def test_motor_busy(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.run_for_time(1000, 127)
				self.assertEqual(port.motor.busy(port.motor.BUSY_MOTOR),True)

	def test_motor_callbacks(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.callback(None)

				mymock = mock.Mock()
				mymock.method.assert_not_called()
				assert port.motor.callback() is None
				port.motor.callback(mymock.method)
				assert callable(port.motor.callback())
				mymock.method.assert_not_called()
				port.motor.brake()
				mymock.method.assert_called_once()

				port.motor.callback(None)

	def test_motor_callbacks_old(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.callback(None)

				global myflag
				myflag = 0
				def myfun(x):
					global myflag
					myflag=1

				assert port.motor.callback() is None
				port.motor.callback(myfun)
				assert callable(port.motor.callback())
				assert myflag==0
				port.motor.brake()
				assert myflag==1

				port.motor.callback(None)

	def test_motor_default_callbacks(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.motor.callback(None)

				mymock = mock.Mock()
				mymock.method.assert_not_called()
				port.motor.default(callback=mymock.method)
				mymock.method.assert_not_called()
				port.motor.brake()
				mymock.method.assert_called_once()

				port.motor.callback(None)

	def test_pwm_values(self):
		for port in self.ports:
			with self.subTest(port=port):
				port.pwm(100)
				port.pwm(-100)
				with self.assertRaises(ValueError):
					port.pwm(101)
				with self.assertRaises(ValueError):
					port.pwm(-101)
				port.pwm(0)


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
