"""Test distance sensor"""

import unittest

from buildhat import DistanceSensor
from buildhat.exc import DeviceError


class TestDistance(unittest.TestCase):
    """Test distance sensor"""

    def test_properties(self):
        """Test properties of sensor"""
        d = DistanceSensor('A')
        self.assertIsInstance(d.distance, int)
        self.assertIsInstance(d.threshold_distance, int)

    def test_distance(self):
        """Test obtaining distance"""
        d = DistanceSensor('A')
        self.assertIsInstance(d.get_distance(), int)

    def test_eyes(self):
        """Test lighting LEDs on sensor"""
        d = DistanceSensor('A')
        d.eyes(100, 100, 100, 100)

    def test_duplicate_port(self):
        """Test using same port"""
        d = DistanceSensor('A')  # noqa: F841
        self.assertRaises(DeviceError, DistanceSensor, 'A')

    def test_del(self):
        """Test deleting sensor"""
        d = DistanceSensor('A')
        del d
        DistanceSensor('A')


if __name__ == '__main__':
    unittest.main()
