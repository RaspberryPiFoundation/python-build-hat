"""Test light functionality"""

import time
import unittest

from buildhat import Light
from buildhat.exc import LightError


class TestLight(unittest.TestCase):
    """Test light functions"""

    def test_light(self):
        """Test light functions"""
        light = Light('A')
        light.on()
        light.brightness(0)
        light.brightness(100)
        time.sleep(1)
        light.brightness(25)
        time.sleep(1)
        self.assertRaises(LightError, light.brightness, -1)
        self.assertRaises(LightError, light.brightness, 101)
        light.off()


if __name__ == '__main__':
    unittest.main()
