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
		assert {'hardware_revision', 'firmware_revision'}.issubset(hub.info().keys())

	def test_port_types(self):
		for P in random.shuffle(hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F):
			assert {'callback', 'device', 'info', 'mode', 'pwm'}.issubset(dir(P))
			assert isinstance(P.info(), dict)
			assert {'type'}.issubset(P.info().keys())

	def test_port_mode_implemented(self):
		for P in random.shuffle(hub.port.A, hub.port.B, hub.port.C, hub.port.D, hub.port.F):
			P.mode()

# These tests must be done with a dummy attached to port A
class PortAttachedTestCase(unittest.TestCase):
	def test_port_info(self):
		assert isinstance(hub.port.A.info(), dict)
		assert {'type', 'fw_version', 'hw_version', 'modes', 'combi_modes'}.issubset(hub.port.A.info().keys())
		assert isinstance(hub.port.A.info()['modes'], list)
		assert {'name', 'raw', 'pct', 'si', 'symbol', 'map_out', 'map_in', 'capability', 'format'}.issubset(hub.port.A.info().keys()['modes'][1])

	def test_port_device_mode(self):
		assert isinstance(hub.port.A.device.mode(), list)

# These tests must be done with nothing attached to port B
class PortDetachedTestCase(unittest.TestCase):
	def test_port_info(self):
		assert isinstance(hub.port.B.info(), dict)
		assert hub.port.B.info() == {'type': None}

	def test_port_device(self):
		assert hub.port.B.device is None
