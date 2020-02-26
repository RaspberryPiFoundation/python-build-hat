import random
import subprocess

try:
# Fake Hat for testing purposes
	fakeHat = subprocess.Popen('/home/max//kynesim/322/FakeHat/bin/FakeHat', stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	import hub # isort:skip
	# Attaching a dummy to port A
	fakeHat.stdin.write(b'attach a $dummy\n')
	fakeHat.stdin.flush()


	# These tests should pass regardless of the state of the hat
	assert {'BT_VCP', 'Image', 'USB_VCP', 'battery', 'ble', 'bluetooth', 'button', 'display', 'firmware', 'info', 'led', 'motion', 'port', 'power_off', 'sound', 'status', 'supervision', 'temperature', 'text'}.issubset(dir(hub))

	assert isinstance(hub.info(), dict)

	assert {'hardware_revision', 'device_uuid'}.issubset(hub.info().keys()) # From real hub

	assert {'BATTERY_BAD_BATTERY', 'BATTERY_HUB_TEMPERATURE_CRITICAL_OUT_OF_RANGE', 'BATTERY_NO_ERROR', 'BATTERY_TEMPERATURE_OUT_OF_RANGE', 'BATTERY_TEMPERATURE_SENSOR_FAIL', 'BATTERY_VOLTAGE_TOO_LOW', 'CHARGER_STATE_CHARGING_COMPLETED', 'CHARGER_STATE_CHARGING_ONGOING', 'CHARGER_STATE_DISCHARGING', 'CHARGER_STATE_FAIL', 'USB_CH_PORT_CDP', 'USB_CH_PORT_DCP', 'USB_CH_PORT_NONE', 'USB_CH_PORT_SDP', 'capacity_left', 'charger_detect', 'current', 'info', 'temperature', 'voltage'}.issubset(dir(hub.battery)) 

	assert {'temperature', 'charge_voltage', 'charge_current', 'charge_voltage_filtered', 'error_state', 'charger_state', 'battery_capacity_left'}.issubset(hub.battery.info().keys())

	assert 0 <= hub.battery.capacity_left() <= 100
	assert 0 <= hub.battery.charger_detect() <= 4 # Number says what kind of charger
	assert 0 <= hub.battery.current() < 100 # In milliamps Assumes nothing is plugged in, goes up when motor is running
	assert 10<= hub.battery.temperature() <= 40 # In C. Assuming we're indoors
	assert 6 <= hub.battery.voltage() <= 10 # Voltage in milivolts

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
		isinstance(hub.port.A.device.mode())
	except NotImpelementedError:
		self.skipTest('Mode not implemented')

	# These tests must be done with nothing attached to port F
	assert isinstance(hub.port.F.info(), dict)
	assert hub.port.F.info() == {'type': None}

	assert hub.port.F.device is None

finally:
	fakeHat.terminate()
	time.sleep(1)
	fakeHat.kill()
