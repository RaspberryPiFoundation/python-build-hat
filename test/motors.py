import unittest
import time
from buildhat.exc import DeviceInvalid, DirectionInvalid, MotorException
from buildhat import Motor

class TestMotorMethods(unittest.TestCase):

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

if __name__ == '__main__':
    unittest.main()
