from .devices import Device
from .exc import DeviceInvalid
from threading import Condition
from collections import deque
import math

class ColorDistanceSensor(Device):
    """Color Distance sensor

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no colordistance sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self.typeid != 37:
            raise DeviceInvalid('There is not a colordistance sensor connected to port %s (Found %s)' % (port, self.name))
        self.on()
        self.mode(6)
        self.avg_reads = 4
        self._old_color = None

    def segment_color(self, r, g, b):
        """Returns the color name from HSV

        :return: Name of the color as a string
        :rtype: str
        """
        table = [("black",(0,0,0)),
                 ("violet",(127,0,255)),
                 ("blue",(0,0,255)),
                 ("cyan",(0,183,235)),
                 ("green",(0,128,0)),
                 ("yellow",(255,255,0)),
                 ("red",(255,0,0)),
                 ("white",(255,255,255))]
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

        :return: HSV representation of color
        :rtype: tuple
        """
        r, g, b = r/255.0, g/255.0, b/255.0
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
        return int(h), int(s*100), int(v*100)

    def get_color(self):
        """Returns the color

        :return: Name of the color as a string
        :rtype: str
        """
        r, g, b = self.get_color_rgb()
        return self.segment_color(r, g, b)

    def get_ambient_light(self):
        """Returns the ambient light

        :return: Ambient light
        :rtype: int
        """
        self.mode(4)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings)/len(readings))
    
    def get_reflected_light(self):
        """Returns the reflected light

        :return: Reflected light
        :rtype: int
        """
        self.mode(3)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings)/len(readings))

    def _clamp(self, val, small, large):
        return max(small, min(val, large))

    def _avgrgb(self, reads):
        readings = []
        for read in reads:
            read = [int((self._clamp(read[0], 0, 400)/400)*255),
                    int((self._clamp(read[1], 0, 400)/400)*255),
                    int((self._clamp(read[2], 0, 400)/400)*255)]
            readings.append(read)
        rgb = []
        for i in range(3):
            rgb.append(int(sum([rgb[i] for rgb in readings]) / len(readings)))
        return rgb

    def get_color_rgb(self):
        """Returns the color 

        :return: RGBI representation 
        :rtype: list
        """
        self.mode(6)
        reads = []
        for i in range(self.avg_reads):
            reads.append(self.get())
        return self._avgrgb(reads)

    def wait_until_color(self, color):
        """Waits until specific color

        :param color: Color to look for 
        """
        self.mode(6)
        cond = Condition()
        data = deque(maxlen=self.avg_reads)
        def cb(lst):
            data.append(lst)
            if len(data) == self.avg_reads:
                r, g, b = self._avgrgb(data)
                seg = self.segment_color(r, g, b)
                if seg == color:
                    with cond:
                        cond.notify()
        self.callback(cb)
        with cond:
            cond.wait()
        self.callback(None)

    def wait_for_new_color(self):
        """Waits for new color or returns immediately if first call
        """
        self.mode(6)
        if self._old_color is None:
            self._old_color = self.get_color()
            return self._old_color
        cond = Condition()
        data = deque(maxlen=self.avg_reads)
        def cb(lst):
            data.append(lst)
            if len(data) ==  self.avg_reads:
                r, g, b = self._avgrgb(data)
                seg = self.segment_color(r, g, b)
                if seg != self._old_color:
                    self._old_color = seg
                    with cond:
                        cond.notify()
        self.callback(cb)
        with cond:
            cond.wait()
        self.callback(None)
        return self._old_color

    def on(self):
        """
        Turns on the sensor and LED
        """
        self._write("port {} ; plimit 1 ; set -1\r".format(self.port))
