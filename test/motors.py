"""Test motors"""

import time
import unittest

from buildhat import Hat, Motor
from buildhat.exc import DeviceError, MotorError


class TestMotor(unittest.TestCase):
    """Test motors"""

    def test_rotations(self):
        """Test motor rotating"""
        m = Motor('A')
        pos1 = m.get_position()
        m.run_for_rotations(2)
        pos2 = m.get_position()
        rotated = (pos2 - pos1) / 360
        self.assertLess(abs(rotated - 2), 0.5)

    def test_position(self):
        """Test motor goes to desired position"""
        m = Motor('A')
        m.run_to_position(0)
        pos1 = m.get_aposition()
        diff = abs((0 - pos1 + 180) % 360 - 180)
        self.assertLess(diff, 10)

        m.run_to_position(180)
        pos1 = m.get_aposition()
        diff = abs((180 - pos1 + 180) % 360 - 180)
        self.assertLess(diff, 10)

    def test_time(self):
        """Test motor runs for correct duration"""
        m = Motor('A')
        t1 = time.time()
        m.run_for_seconds(5)
        t2 = time.time()
        self.assertEqual(int(t2 - t1), 5)

    def test_speed(self):
        """Test setting motor speed"""
        m = Motor('A')
        m.set_default_speed(50)
        self.assertRaises(MotorError, m.set_default_speed, -101)
        self.assertRaises(MotorError, m.set_default_speed, 101)

    def test_plimit(self):
        """Test altering power limit of motor"""
        m = Motor('A')
        m.plimit(0.5)
        self.assertRaises(MotorError, m.plimit, -1)
        self.assertRaises(MotorError, m.plimit, 2)

    def test_bias(self):
        """Test setting motor bias"""
        m = Motor('A')
        m.bias(0.5)
        self.assertRaises(MotorError, m.bias, -1)
        self.assertRaises(MotorError, m.bias, 2)

    def test_pwm(self):
        """Test PWMing motor"""
        m = Motor('A')
        m.pwm(0.3)
        time.sleep(0.5)
        m.pwm(0)
        self.assertRaises(MotorError, m.pwm, -2)
        self.assertRaises(MotorError, m.pwm, 2)

    def test_callback(self):
        """Test setting callback"""
        m = Motor('A')

        def handle_motor(speed, pos, apos):
            handle_motor.evt += 1
        handle_motor.evt = 0
        m.when_rotated = handle_motor
        m.run_for_seconds(1)
        self.assertGreater(handle_motor.evt, 0)

    def test_none_callback(self):
        """Test setting empty callback"""
        m = Motor('A')
        m.when_rotated = None
        m.start()
        time.sleep(0.5)
        m.stop()

    def test_duplicate_port(self):
        """Test using same port for motor"""
        m1 = Motor('A')  # noqa: F841
        self.assertRaises(DeviceError, Motor, 'A')

    def test_del(self):
        """Test deleting motor"""
        m1 = Motor('A')
        del m1
        Motor('A')

    def test_continuous_start(self):
        """Test starting motor for 5mins"""
        t = time.time() + (60 * 5)
        m = Motor('A')
        while time.time() < t:
            m.start(0)

    def test_continuous_degrees(self):
        """Test setting degrees for 5mins"""
        t = time.time() + (60 * 5)
        m = Motor('A')
        while time.time() < t:
            m.run_for_degrees(0)

    def test_continuous_position(self):
        """Test setting position of motor for 5mins"""
        t = time.time() + (60 * 5)
        m = Motor('A')
        while time.time() < t:
            m.run_to_position(0)

    def test_continuous_feedback(self):
        """Test feedback of motor for 30mins"""
        Hat(debug=True)
        t = time.time() + (60 * 30)
        m = Motor('A')
        m.start(40)
        while time.time() < t:
            _ = (m.get_speed(), m.get_position(), m.get_aposition())


if __name__ == '__main__':
    unittest.main()
