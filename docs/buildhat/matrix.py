"""Example driving LED matrix"""

import random
import time

from buildhat import Matrix

matrix = Matrix('C')

matrix.clear(("red", 10))
time.sleep(1)

matrix.clear()
time.sleep(1)

matrix.set_pixel((0, 0), ("blue", 10))
matrix.set_pixel((2, 2), ("red", 10))
time.sleep(1)

while True:
    out = [[(int(random.uniform(0, 9)), 10) for x in range(3)] for y in range(3)]
    matrix.set_pixels(out)
    time.sleep(0.1)
