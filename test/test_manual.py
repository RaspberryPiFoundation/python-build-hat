import random
import resource
import unittest

# Start fakeHat manually before this line
import hub


# These tests should pass regardless of the state of the hat
class GeneralTestCase(unittest.TestCase):
	def test_info_type(self):
		assert {'info', 'port'}.issubset(dir(hub))
		assert isinstance(hub.info(), dict)
		assert len(hub.info())>=2
		assert {'hardware_revision', 'device_uuid'}.issubset(hub.info().keys()) # From real hub

	def test_port_types(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
			assert isinstance(P.info(), dict)
			assert {'type'}.issubset(P.info().keys())

	@unittest.skip("Not implemented")
	def test_port_mode_implemented(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			P.mode()

# These tests must be done with a dummy attached to port A
class PortAttachedTestCase(unittest.TestCase):
	def test_port_info(self):
		assert isinstance(hub.port.A.info(), dict)
		assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(hub.port.A.info().keys())
		assert isinstance(hub.port.A.info()['modes'], list)
		assert {'name', 'raw', 'pct', 'si', 'symbol', 'map_out', 'map_in', 'capability', 'format'}.issubset(hhub.port.A.info()['modes'][1].keys())

	def test_port_device_mode(self):
		assert {'mode'}.issubset(dir(hub.port.A.device))
		try:
			isinstance(hub.port.A.device.mode())
		except NotImpelementedError:
			self.skipTest('Mode not implemented')

# These tests must be done with nothing attached to port B
class PortDetachedTestCase(unittest.TestCase):
	def test_port_info(self):
		assert isinstance(hub.port.B.info(), dict)
		assert hub.port.B.info() == {'type': None}

	def test_port_device(self):
		assert hub.port.B.device is None

# These tests were designed on the hub itself
class HubFunctionalityTestCase(unittest.TestCase):
	def test_hub_functionality(self):
		{'BT_VCP', 'Image', 'USB_VCP', 'battery', 'ble', 'bluetooth', 'button', 'display', 'firmware', 'info', 'led', 'motion', 'port', 'power_off', 'sound', 'status', 'supervision', 'temperature', 'text'}.issubset(dir(hub))
		assert {'hardware_revision', 'device_uuid'}.issubset(hub.info().keys())

	def test_port_functionality(self):
		ports = [hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F]
		random.shuffle(ports)
		for P in ports:
			assert {'callback', 'device', 'info', 'mode', 'motor', 'pwm'}.issubset(dir(P)) 
			assert isinstance(P.info(), dict)
			assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(P.info().keys())

	# Motor must be connected to port A
	def test_motor_A(self):
		assert {'BUSY_MODE', 'BUSY_MOTOR', 'EVENT_COMPLETED', 'EVENT_INTERRUPTED', 'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'PID_POSITION', 'PID_SPEED', 'STOP_BRAKE', 'STOP_FLOAT', 'STOP_HOLD', 'brake', 'busy', 'callback', 'default', 'float', 'get', 'hold', 'mode', 'pair', 'pid', 'preset', 'pwm', 'run_at_speed', 'run_for_degrees', 'run_for_time', 'run_to_position'}.issubset(dir(hub.motor.A))
		hub.port.A.motor.run_for_time(1000, 127) # run for 1000ms at maximum clockwise speed
		hub.port.A.motor.run_for_time(1000, -127) # run for 1000ms at maximum anticlockwise speed
		hub.port.A.motor.run_for_degrees(180, 127) # turn 180 degrees clockwise at maximum speed
		hub.port.A.motor.run_for_degrees(720, -127) # Make two rotations anticlockwise at maximum speed
		hub.port.A.motor.run_to_position(0, 127) # Move to top dead centre at maximum speed (positioning seems to be absolute)
		hub.port.A.motor.run_to_position(180, 127) # Move to 180 degrees forward of top dead centre at maximum speed

	# Touch sensor must be connected to port B
	def test_touch_sensor_B(self)
		assert {'FORMAT_PCT', 'FORMAT_RAW', 'FORMAT_SI', 'get', 'mode', 'pwm'}.issubset(dir(hub.port.B.device))
		assert hub.port.B.device.get() <= 1 # Without button pressed
		assert hub.port.B.device.get() >= 0 # At all times
		assert hub.port.B.device.get() <= 9 # At all times

if __name__ == '__main__':
    unittest.main()
