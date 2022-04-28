"""Test matrix functionality"""

import time
import unittest

from buildhat import Matrix
from buildhat.exc import MatrixError


class TestMatrix(unittest.TestCase):
    """Test matrix functions"""

    def test_matrix(self):
        """Test setting matrix pixels"""
        matrix = Matrix('A')
        matrix.set_pixels([[(10, 10) for x in range(3)] for y in range(3)])
        time.sleep(1)
        self.assertRaises(MatrixError, matrix.set_pixels, [[(10, 10) for x in range(3)] for y in range(4)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[(10, 10) for x in range(4)] for y in range(3)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[(11, 10) for x in range(3)] for y in range(3)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[(-1, 10) for x in range(3)] for y in range(3)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[(10, 11) for x in range(3)] for y in range(3)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[(10, -1) for x in range(3)] for y in range(3)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[("gold", 10) for x in range(3)] for y in range(3)])
        self.assertRaises(MatrixError, matrix.set_pixels, [[(10, "test") for x in range(3)] for y in range(3)])
        matrix.set_pixels([[("pink", 10) for x in range(3)] for y in range(3)])
        time.sleep(1)

    def test_clear(self):
        """Test clearing matrix"""
        matrix = Matrix('A')
        matrix.clear()
        matrix.clear((10, 10))
        time.sleep(1)
        matrix.clear(("yellow", 10))
        time.sleep(1)
        self.assertRaises(MatrixError, matrix.clear, ("gold", 10))
        self.assertRaises(MatrixError, matrix.clear, (10, -1))
        self.assertRaises(MatrixError, matrix.clear, (10, 11))
        self.assertRaises(MatrixError, matrix.clear, (-1, 10))
        self.assertRaises(MatrixError, matrix.clear, (10, 11))

    def test_transition(self):
        """Test transitions"""
        matrix = Matrix('A')
        matrix.clear(("green", 10))
        time.sleep(1)
        matrix.set_transition(1)
        matrix.set_pixels([[("blue", 10) for x in range(3)] for y in range(3)])
        time.sleep(4)
        matrix.set_transition(2)
        matrix.set_pixels([[("red", 10) for x in range(3)] for y in range(3)])
        time.sleep(4)
        matrix.set_transition(0)
        self.assertRaises(MatrixError, matrix.set_transition, -1)
        self.assertRaises(MatrixError, matrix.set_transition, 3)
        self.assertRaises(MatrixError, matrix.set_transition, "test")

    def test_level(self):
        """Test level"""
        matrix = Matrix('A')
        matrix.clear(("orange", 10))
        time.sleep(1)
        matrix.level(5)
        time.sleep(4)
        self.assertRaises(MatrixError, matrix.level, -1)
        self.assertRaises(MatrixError, matrix.level, 10)
        self.assertRaises(MatrixError, matrix.level, "test")

    def test_pixel(self):
        """Test pixel"""
        matrix = Matrix('A')
        matrix.clear()
        matrix.set_pixel((0, 0), ("red", 10))
        matrix.set_pixel((2, 2), ("red", 10))
        time.sleep(1)
        self.assertRaises(MatrixError, matrix.set_pixel, (-1, 0), ("red", 10))
        self.assertRaises(MatrixError, matrix.set_pixel, (0, -1), ("red", 10))
        self.assertRaises(MatrixError, matrix.set_pixel, (3, 0), ("red", 10))
        self.assertRaises(MatrixError, matrix.set_pixel, (0, 3), ("red", 10))
        self.assertRaises(MatrixError, matrix.set_pixel, (0, 0), ("gold", 10))
        self.assertRaises(MatrixError, matrix.set_pixel, (0, 0), ("red", -1))
        self.assertRaises(MatrixError, matrix.set_pixel, (0, 0), ("red", 11))


if __name__ == '__main__':
    unittest.main()
