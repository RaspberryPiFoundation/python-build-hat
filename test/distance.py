import unittest
from buildhat import DistanceSensor


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

if __name__ == '__main__':
    unittest.main()
