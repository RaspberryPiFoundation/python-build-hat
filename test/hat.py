import unittest

from buildhat import Hat


class TestHat(unittest.TestCase):

    def test_vin(self):
        h = Hat()
        vin = h.get_vin()
        self.assertGreaterEqual(vin, 7.2)
        self.assertLessEqual(vin, 8.5)

    def test_get(self):
        h = Hat()
        self.assertIsInstance(h.get(), dict)

    def test_serial(self):
        Hat(device="/dev/serial0")


if __name__ == '__main__':
    unittest.main()
