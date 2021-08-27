from .devices import Device
from .exc import DeviceInvalid
import threading
import time

class Matrix(Device):
    """LED Matrix

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no led matrix attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self.typeid != 64:
            raise DeviceInvalid('There is not a led matrix connected to port %s (Found %s)' % (port, self.name))
        self.on()
        self.mode(2)
        self._matrix = [[(0,0) for x in range(3)] for y in range(3)]

    def set_pixels(self, matrix):
        """Write pixel data to LED matrix

        :param pixels: 3x3 list of tuples, with color (0-9) and brightness (0-10) (see example for more detail)
        """
        for x in range(3):
            for y in range(3):
                color, brightness = matrix[x][y]
                if not (brightness >= 0 and brightness <= 10):
                    raise MatrixInvalidPixel("Invalid brightness specified")
                if not (color >= 0 and color <= 9):
                    raise MatrixInvalidPixel("Invalid pixel specified")
        self._matrix = matrix
        self._output()

    def _output(self):
        out = [0xc2]
        for x in range(3):
            for y in range(3):
                out.append((self._matrix[x][y][1] << 4) | self._matrix[x][y][0])
        self.select()
        self._write1(out)
        self.deselect()

    def strtocolor(self, colorstr):
        if colorstr == "pink":
            return 1
        elif colorstr == "lilac":
            return 2
        elif colorstr == "blue":
            return 3
        elif colorstr == "cyan":
            return 4
        elif colorstr == "turquoise":
            return 5
        elif colorstr == "green":
            return 6
        elif colorstr == "yellow":
            return 7
        elif colorstr == "orange":
            return 8
        elif colorstr == "red":
            return 9
        raise MatrixInvalidPixel("Invalid color specified")

    def clear(self, pixel=None):
        """Clear matrix or set all as the same pixel

        :param pixel: tuple of color (0-9) or string and brightness (0-10)
        """
        if pixel is None:
            self._matrix = [[(0,0) for x in range(3)] for y in range(3)]
        else:
            color = ()
            if isinstance(pixel, tuple):
                c, brightness = pixel
                if isinstance(c, str):
                    c = self.strtocolor(c)
                if not (brightness >= 0 and brightness <= 10):
                    raise MatrixInvalidPixel("Invalid brightness specified")
                if not (c >= 0 and c <= 9):
                    raise MatrixInvalidPixel("Invalid pixel specified")
                color = (c, brightness)
            else:
                raise MatrixInvalidPixel("Invalid pixel specified")
            self._matrix = [[color for x in range(3)] for y in range(3)]
        self._output()

    def set_pixel(self, coord, pixel, display=True):
        """Write pixel to coordinate

        :param coord: (0,0) to (2,2)
        :param pixel: tuple of color (0-9) or string and brightness (0-10)
        :param display: Whether to update matrix or not
        """
        if isinstance(pixel, tuple):
            c, brightness = pixel
            if isinstance(c, str):
                c = self.strtocolor(c)
            if not (brightness >= 0 and brightness <= 10):
                raise MatrixInvalidPixel("Invalid brightness specified")
            if not (c >= 0 and c <= 9):
                raise MatrixInvalidPixel("Invalid pixel specified")
            color = (c, brightness)
        else:
            raise MatrixInvalidPixel("Invalid pixel specified")
        if isinstance(coord, tuple):
            if not (isinstance(coord[0], int) and isinstance(coord[1], int)):
                raise MatrixInvalidPixel("Invalid coord specified")
        else:
            raise MatrixInvalidPixel("Invalid coord specified")
        x, y = coord
        self._matrix[x][y] = color
        if display:
            self._output()
