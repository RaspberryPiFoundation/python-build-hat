from buildhat import Matrix
import time
import random

matrix = Matrix('C')

while True:
    pixels = []
    for pixel in range(9): 
        pixels += [(int(random.uniform(0,9)),10)]
    matrix.write(pixels)
    time.sleep(0.1)
