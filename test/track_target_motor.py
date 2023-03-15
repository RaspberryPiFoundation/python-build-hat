"""Test motors with ~TargetTrackerMotor"""

import time
import unittest

from buildhat import TargetTrackerMotor


class TestTargetTrackerMotor(unittest.TestCase):
    """Test ~TargetTrackerMotor"""

    def test_command_overwrites(self):
        """Test motor rotating"""
        m = TargetTrackerMotor('A')

        # position motor at 0 degrees
        pos1 = m.get_aposition()
        t0 = time.time()
        while pos1 < -5 or pos1 > 5:
            m.run_to_position(0)
            time.sleep(0.1)
            pos1 = m.get_aposition()
            if time.time() - t0 > 3:
                self.fail("Motor could not reach start position in given time!")

        angles = [0, 90, 0, 90, 0]

        max_angle_reached = 0

        for _ in range(5):
            for angle in angles:
                m.run_to_position(angle)
                time.sleep(0.1)
                pos2 = m.get_aposition()
                if max_angle_reached < pos2:
                    max_angle_reached = pos2

        self.assertLess(max_angle_reached, 45)


if __name__ == '__main__':
    unittest.main()
