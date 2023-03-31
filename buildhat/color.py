"""Color sensor handling functionality"""

import math
from collections import deque
from threading import Condition

from .devices import Device


class ColorSensor(Device):
    """Color sensor

    :param port: Port of device
    :raises DeviceError: Occurs if there is no color sensor attached to port
    """

    def __init__(self, port):
        """
        Initialise color sensor

        :param port: Port of device
        """
        super().__init__(port)
        self.reverse()
        self.mode(6)
        self.avg_reads = 4
        self._old_color = None

    def segment_color(self, r, g, b):
        """Return the color name from RGB

        :param r: Red
        :param g: Green
        :param b: Blue
        :return: Name of the color as a string
        :rtype: str
        """
        table = [("black", (0, 0, 0)),
                 ("violet", (127, 0, 255)),
                 ("blue", (0, 0, 255)),
                 ("cyan", (0, 183, 235)),
                 ("green", (0, 128, 0)),
                 ("yellow", (255, 255, 0)),
                 ("red", (255, 0, 0)),
                 ("white", (255, 255, 255))]
        near = ""
        euc = math.inf
        for itm in table:
            cur = math.sqrt((r - itm[1][0])**2 + (g - itm[1][1])**2 + (b - itm[1][2])**2)
            if cur < euc:
                near = itm[0]
                euc = cur
        return near

    def rgb_to_hsv(self, r, g, b):
        """Convert RGB to HSV

        Based on https://www.rapidtables.com/convert/color/rgb-to-hsv.html algorithm

        :param r: Red
        :param g: Green
        :param b: Blue
        :return: HSV representation of color
        :rtype: tuple
        """
        r, g, b = r / 255.0, g / 255.0, b / 255.0
        cmax = max(r, g, b)
        cmin = min(r, g, b)
        delt = cmax - cmin
        if cmax == cmin:
            h = 0
        elif cmax == r:
            h = 60 * (((g - b) / delt) % 6)
        elif cmax == g:
            h = 60 * ((((b - r) / delt)) + 2)
        elif cmax == b:
            h = 60 * ((((r - g) / delt)) + 4)
        if cmax == 0:
            s = 0
        else:
            s = delt / cmax
        v = cmax
        return int(h), int(s * 100), int(v * 100)

    def get_color(self):
        """Return the color

        :return: Name of the color as a string
        :rtype: str
        """
        r, g, b, _ = self.get_color_rgbi()
        return self.segment_color(r, g, b)

    def get_ambient_light(self):
        """Return the ambient light

        :return: Ambient light
        :rtype: int
        """
        self.mode(2)
        readings = []
        for _ in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings) / len(readings))

    def get_reflected_light(self):
        """Return the reflected light

        :return: Reflected light
        :rtype: int
        """
        self.mode(1)
        readings = []
        for _ in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings) / len(readings))

    def _avgrgbi(self, reads):
        readings = []
        for read in reads:
            read = [int((read[0] / 1024) * 255),
                    int((read[1] / 1024) * 255),
                    int((read[2] / 1024) * 255),
                    int((read[3] / 1024) * 255)]
            readings.append(read)
        rgbi = []
        for i in range(4):
            rgbi.append(int(sum([rgbi[i] for rgbi in readings]) / len(readings)))
        return rgbi

    def get_color_rgbi(self):
        """Return the color

        :return: RGBI representation
        :rtype: list
        """
        self.mode(5)
        reads = []
        for _ in range(self.avg_reads):
            reads.append(self.get())
        return self._avgrgbi(reads)

    def get_color_hsv(self):
        """Return the color

        :return: HSV representation
        :rtype: tuple
        """
        self.mode(6)
        readings = []
        for _ in range(self.avg_reads):
            read = self.get()
            read = [read[0], int((read[1] / 1024) * 100), int((read[2] / 1024) * 100)]
            readings.append(read)
        s = c = 0
        for hsv in readings:
            hue = hsv[0]
            s += math.sin(math.radians(hue))
            c += math.cos(math.radians(hue))

        hue = int((math.degrees((math.atan2(s, c))) + 360) % 360)
        sat = int(sum([hsv[1] for hsv in readings]) / len(readings))
        val = int(sum([hsv[2] for hsv in readings]) / len(readings))
        return (hue, sat, val)

    def _cb_handle(self, lst):
        self._data.append(lst[:4])
        if len(self._data) == self.avg_reads:
            r, g, b, _ = self._avgrgbi(self._data)
            seg = self.segment_color(r, g, b)
            if self._cmp(seg, self._color):
                with self._cond:
                    self._old_color = seg
                    self._cond.notify()

    def wait_until_color(self, color):
        """Wait until specific color

        :param color: Color to look for
        """
        self.mode(5)
        self._cond = Condition()
        self._data = deque(maxlen=self.avg_reads)
        self._color = color
        self._cmp = lambda x, y: x == y
        self.callback(self._cb_handle)
        with self._cond:
            self._cond.wait()
        self.callback(None)

    def wait_for_new_color(self):
        """Wait for new color or returns immediately if first call

        :return: Name of the color as a string
        :rtype: str
        """
        self.mode(5)
        if self._old_color is None:
            self._old_color = self.get_color()
            return self._old_color
        self._cond = Condition()
        self._data = deque(maxlen=self.avg_reads)
        self._color = self._old_color
        self._cmp = lambda x, y: x != y
        self.callback(self._cb_handle)
        with self._cond:
            self._cond.wait()
        self.callback(None)
        return self._old_color

    def on(self):
        """Turn on the sensor and LED"""
        self.reverse()
