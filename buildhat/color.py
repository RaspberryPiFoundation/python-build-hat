from .devices import Device
from .exc import DeviceInvalid
from threading import Condition
from collections import deque
import math

class ColorSensor(Device):
    """Color sensor

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no color sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self.typeid != 61:
            raise DeviceInvalid('There is not a color sensor connected to port %s (Found %s)' % (port, self.name))
        self.reverse()
        self.mode(6)
        self.avg_reads = 4
        self._old_color = None

    def segment_color(self, r, g, b):
        """Returns the color name from HSV

        :return: Name of the color as a string
        :rtype: str
        """
        table = [("white",(100,100,100)),
                 ("silver",(75,75,75)),
                 ("gray",(50,50,50)),
                 ("black",(0,0,0)),
                 ("red",(100,0,0)),
                 ("maroon",(50,0,0)),
                 ("yellow",(100,100,0)),
                 ("olive",(50,50,0)),
                 ("lime",(0,100,0)),
                 ("green",(0,50,0)),
                 ("aqua",(0,100,100)),
                 ("teal",(0,50,50)),
                 ("blue",(0,0,100)),
                 ("navy",(100,0,100)),
                 ("fuschia",(100,0,100)),
                 ("purple",(50,0,50))]
        near = ""
        euc = math.inf
        for itm in table:
            cur = math.sqrt((r - int(itm[1][0]*2.55))**2 + (g - int(itm[1][1]*2.55))**2 + (b - int(itm[1][2]*2.55))**2)
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
        r, g, b, _ = self.get_color_rgbi()
        return self.segment_color(r, g, b)

    def get_ambient_light(self):
        """Returns the ambient light

        :return: Ambient light
        :rtype: int
        """
        self.mode(2)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings)/len(readings))
    
    def get_reflected_light(self):
        """Returns the reflected light

        :return: Reflected light
        :rtype: int
        """
        self.mode(1)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings)/len(readings))

    def _avgrgbi(self, reads):
        readings = []
        for read in reads:
            read = [int((read[0]/1024)*255), int((read[1]/1024)*255), int((read[2]/1024)*255), int((read[3]/1024)*255)]
            readings.append(read)
        rgbi = []
        for i in range(4):
            rgbi.append(int(sum([rgbi[i] for rgbi in readings]) / len(readings)))
        return rgbi

    def get_color_rgbi(self):
        """Returns the color 

        :return: RGBI representation 
        :rtype: tuple
        """
        self.mode(5)
        reads = []
        for i in range(self.avg_reads):
            reads.append(self.get())
        return self._avgrgbi(reads)

    def get_color_hsv(self):
        """Returns the color 

        :return: HSV representation 
        :rtype: tuple
        """
        self.mode(6)
        readings = []
        for i in range(self.avg_reads):
            read = self.get()
            read = [read[0], int((read[1]/1024)*100), int((read[2]/1024)*100)]
            readings.append(read)
        s = c = 0
        for hsv in readings:
            hue = hsv[0]
            s += math.sin(math.radians(hue))
            c += math.cos(math.radians(hue))    

        hue = int((math.degrees((math.atan2(s,c))) + 360) % 360)
        sat = int(sum([hsv[1] for hsv in readings]) / len(readings))
        val = int(sum([hsv[2] for hsv in readings]) / len(readings))
        return (hue, sat, val)       

    def wait_until_color(self, color):
        """Waits until specific color

        :param color: Color to look for 
        """
        self.mode(5)
        cond = Condition()
        data = deque(maxlen=self.avg_reads)
        def cb(lst):
            data.append(lst[:4])
            if len(data) == self.avg_reads:
                r, g, b, _ = self._avgrgbi(data)
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
        self.mode(5)
        if self._old_color is None:
            self._old_color = self.get_color()
            return self._old_color
        cond = Condition()
        data = deque(maxlen=self.avg_reads)
        def cb(lst):
            data.append(lst[:4])
            if len(data) ==  self.avg_reads:
                r, g, b, _ = self._avgrgbi(data)
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
        self._write("port {} ; plimit 1 ; set -1\r".format(self.port))
