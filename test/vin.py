import unittest
from buildhat import Hat


class TestVin(unittest.TestCase):

    def test_vin(self):
        h = Hat()
        vin = h.get_vin()
        self.assertGreaterEqual(vin, 7.2)
        self.assertLessEqual(vin, 8.5)


if __name__ == '__main__':
    unittest.main()
