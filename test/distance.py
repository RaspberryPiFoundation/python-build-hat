import unittest

from buildhat import DistanceSensor
from buildhat.exc import PortInUse


class TestDistance(unittest.TestCase):

    def test_properties(self):
        d = DistanceSensor('A')
        self.assertIsInstance(d.distance, int)
        self.assertIsInstance(d.threshold_distance, int)

    def test_distance(self):
        d = DistanceSensor('A')
        self.assertIsInstance(d.get_distance(), int)

    def test_eyes(self):
        d = DistanceSensor('A')
        d.eyes(100, 100, 100, 100)

    def test_duplicate_port(self):
        d = DistanceSensor('A')
        self.assertRaises(PortInUse, DistanceSensor, 'A')

    def test_del(self):
        d = DistanceSensor('A')
        del d
        d = DistanceSensor('A')


if __name__ == '__main__':
    unittest.main()
