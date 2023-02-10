"""Test Color Sensor functionality"""
import time
import unittest

from buildhat import ColorSensor


class TestColor(unittest.TestCase):
    """Test color sensor functions"""

    def test_color_interval(self):
        """Test color sensor interval"""
        color = ColorSensor('A')
        color.avg_reads = 1
        color.interval = 10
        count = 1000
        expected_dur = count * color.interval * 1e-3

        start = time.time()
        for _ in range(count):
            color.get_ambient_light()
        end = time.time()
        diff = abs((end - start) - expected_dur)
        self.assertLess(diff, 0.25)

        start = time.time()
        for _ in range(count):
            color.get_color_rgbi()
        end = time.time()
        diff = abs((end - start) - expected_dur)
        self.assertLess(diff, 0.25)

    def test_caching(self):
        """Test to make sure we're not reading cached data"""
        color = ColorSensor('A')
        color.avg_reads = 1
        color.interval = 1

        for _ in range(100):
            color.mode(2)
            self.assertEqual(len(color.get()), 1)
            color.mode(5)
            self.assertEqual(len(color.get()), 4)


if __name__ == '__main__':
    unittest.main()
