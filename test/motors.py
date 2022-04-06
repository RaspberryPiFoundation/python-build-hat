import unittest
import time
from buildhat.exc import DeviceInvalid, DirectionInvalid, MotorException, PortInUse
from buildhat import Motor

class TestMotor(unittest.TestCase):

    def test_rotations(self):
        m = Motor('A')
        pos1 = m.get_position()
        m.run_for_rotations(2) 
        pos2 = m.get_position()
        rotated = (pos2 - pos1)/360
        self.assertLess(abs(rotated - 2), 0.5)

    def test_position(self):
        m = Motor('A')
        m.run_to_position(0)
        pos1 = m.get_aposition()
        diff = abs((0-pos1+180) % 360 - 180)
        self.assertLess(diff, 10)

        m.run_to_position(180)
        pos1 = m.get_aposition()
        diff = abs((180-pos1+180) % 360 - 180)
        self.assertLess(diff, 10)

    def test_time(self):
        m = Motor('A')
        t1 = time.time()
        m.run_for_seconds(5)
        t2 = time.time()
        self.assertEqual(int(t2 - t1), 5)

    def test_speed(self):
        m = Motor('A')
        m.set_default_speed(50)
        self.assertRaises(MotorException, m.set_default_speed, -101)
        self.assertRaises(MotorException, m.set_default_speed, 101)

    def test_plimit(self):
        m = Motor('A')
        m.plimit(0.5)
        self.assertRaises(MotorException, m.plimit, -1)
        self.assertRaises(MotorException, m.plimit, 2)

    def test_bias(self):
        m = Motor('A')
        m.bias(0.5)
        self.assertRaises(MotorException, m.bias, -1)
        self.assertRaises(MotorException, m.bias, 2)

    def test_pwm(self):
        m = Motor('A')
        m.pwm(0.3)
        time.sleep(0.5)
        m.pwm(0)
        self.assertRaises(MotorException, m.pwm, -2)
        self.assertRaises(MotorException, m.pwm, 2)

    def test_callback(self):
        m = Motor('A')
        def handle_motor(speed, pos, apos):
            handle_motor.evt += 1
        handle_motor.evt = 0
        m.when_rotated = handle_motor
        m.run_for_seconds(1)
        self.assertGreater(handle_motor.evt, 0)

    def test_none_callback(self):
        m = Motor('A')
        m.when_rotated = None
        m.start()
        time.sleep(0.5)
        m.stop()

    def test_duplicate_port(self):
        m1 = Motor('A')
        self.assertRaises(PortInUse, Motor, 'A')

    def test_del(self):
        m1 = Motor('A')
        del m1
        m1 = Motor('A')

    def test_continuous_start(self):
        t = time.time() + (60*5)
        m = Motor('A')
        while time.time() < t:
            m.start(0)

    def test_continuous_degrees(self):
        t = time.time() + (60*5)
        m = Motor('A')
        while time.time() < t:
            m.run_for_degrees(0)

    def test_continuous_position(self):
        t = time.time() + (60*5)
        m = Motor('A')
        while time.time() < t:
            m.run_to_position(0)

if __name__ == '__main__':
    unittest.main()
