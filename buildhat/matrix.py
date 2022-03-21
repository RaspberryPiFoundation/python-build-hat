from .devices import Device
from .exc import DeviceInvalid, MatrixInvalidPixel
import threading
import time

class Matrix(Device):
    """LED Matrix

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no LED matrix attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        self.on()
        self.mode(2)
        self._matrix = [[(0,0) for x in range(3)] for y in range(3)]

    def set_pixels(self, matrix):
        """Write pixel data to LED matrix

        :param pixels: 3x3 list of tuples, with colour (0–9) and brightness (0–10) (see example for more detail)
        """
        if len(matrix) != 3:
            raise MatrixInvalidPixel("Incorrect matrix height")
        for x in range(3):
            if len(matrix[x]) != 3:
                raise MatrixInvalidPixel("Incorrect matrix width")
            for y in range(3):
                matrix[x][y] = Matrix.normalize_pixel(matrix[x][y])
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

    def strtocolor(colorstr):
        """Return the BuldHAT's integer representation of a color string

        :param colorstr: str of a valid color
        :return: (0-10) representing the color
        :rtype: int
        """
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
        elif colorstr == "white":
            return 10
        elif colorstr == "":
            return 0
        raise MatrixInvalidPixel("Invalid color specified")

    def normalize_pixel(pixel):
        """Validate a pixel tuple (color, brightness) and convert string colors to integers

        :param pixel: tuple of colour (0–10) or string (ie:"red") and brightness (0–10)
        :return: (color, brightness) integers
        :rtype: tuple
        """
        if isinstance(pixel, tuple):
            c, brightness = pixel
            if isinstance(c, str):
                c = Matrix.strtocolor(c)
            if not ( isinstance(brightness, int) and isinstance(c, int)):
                raise MatrixInvalidPixel("Invalid pixel specified")
            if not (brightness >= 0 and brightness <= 10):
                raise MatrixInvalidPixel("Invalid brightness value specified")
            if not (c >= 0 and c <= 10):
                raise MatrixInvalidPixel("Invalid pixel color specified")
            return (c, brightness)
        else:
            raise MatrixInvalidPixel("Invalid pixel specified")	

    def validate_coordinate(coord):
        """"Validate an x,y coordinate for the 3x3 Matrix

        :param coord: tuple of 0-3 for the X coordinate and 0-2 for the Y coordinate
        """
        if isinstance(coord, tuple):
            if not (isinstance(coord[0], int) and isinstance(coord[1], int)):
                raise MatrixInvalidPixel("Invalid coord specified")
            elif coord[0] > 2 or coord[0] < 0 or coord[1] > 2 or coord[1] < 0:
            	raise MatrixInvalidPixel("Invalid coord specified")
        else:
            raise MatrixInvalidPixel("Invalid coord specified")

    def clear(self, pixel=None):
        """Clear matrix or set all as the same pixel

        :param pixel: tuple of colour (0–10) or string and brightness (0–10)
        """
        if pixel is None:
            self._matrix = [[(0,0) for x in range(3)] for y in range(3)]
        else:
            color = Matrix.normalize_pixel(pixel)
            self._matrix = [[color for x in range(3)] for y in range(3)]
        self._output()

    def off(self):
        # Never send the "off" command to the port a Matrix is connected to
        # Instead, just turn all the pixels off
        self.clear()

    def level(self, level):
        """Use the matrix as a "level" meter from 0-9
        (The level meter is expressed in green which seems to be unchangeable)

        :param level: The height of the bar graph, 0-9
        """
        if not isinstance(level, int):
            raise MatrixInvalidPixel("Invalid level, not integer")
        if not (level >= 0 and level <= 9):
            raise MatrixInvalidPixel("Invalid level specified")
        self.mode(0)
        self.select()
        self._write1([0xc0, level])
        self.mode(2)  # The rest of the Matrix code seems to expect this to be always set
        self.deselect()

    def set_transition(self, transition):
        """Set the transition mode between pixels

        Use display=False on set_pixel() or use set_pixels() to achieve desired
        results with transitions.

        Setting a new transition mode will wipe the screen and interrupt any
        running transition.

        Mode 0: No transition, immediate pixel drawing

        Mode 1: Right-to-left wipe in/out

        If the timing between writing new matrix pixels is less than one second
        the transition will clip columns of pixels from the right.

        Mode 2: Fade-in/Fade-out

        The fade in and fade out take about 2.2 seconds for full fade effect.
        Waiting less time between setting new pixels will result in a faster
        fade which will cause the fade to "pop" in brightness.

        :param transition: Transition mode (0-2)
        """
        if not isinstance(transition, int):
            raise MatrixInvalidPixel("Invalid transition, not integer")
        if not (transition >= 0 and transition <= 2):
            raise MatrixInvalidPixel("Invalid transition specified")
        self.mode(3)
        self.select()
        self._write1([0xc3, transition])
        self.mode(2)  # The rest of the Matrix code seems to expect this to be always set
        self.deselect()

    def set_pixel(self, coord, pixel, display=True):
        """Write pixel to coordinate

        :param coord: (0,0) to (2,2)
        :param pixel: tuple of colour (0–10) or string and brightness (0–10)
        :param display: Whether to update matrix or not
        """
        color = Matrix.normalize_pixel(pixel)
        Matrix.validate_coordinate(coord)
        x, y = coord
        self._matrix[x][y] = color
        if display:
            self._output()
