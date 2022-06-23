"""Test wedo sensor functionality"""

import unittest

from buildhat import MotionSensor, TiltSensor


class TestWeDo(unittest.TestCase):
    """Test wedo sensor functions"""

    def test_motionsensor(self):
        """Test motion sensor"""
        motion = MotionSensor('A')
        dist = motion.get_distance()
        self.assertIsInstance(dist, int)

    def test_tiltsensor(self):
        """Test tilt sensor

        ToDo - Test when I re-find this sensor
        """
        tilt = TiltSensor('B')
        tvalue = tilt.get_tilt()
        self.assertIsInstance(tvalue, tuple)
        self.assertIsInstance(tvalue[0], int)
        self.assertIsInstance(tvalue[1], int)


if __name__ == '__main__':
    unittest.main()
