from .devices import PortDevice
import math
import threading

class ColorSensor(PortDevice):
    """Color sensor

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no color sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        self._device.mode(6)
        self.avg_reads = 15
        self._old_color = None

    def segment_color(self, h, s, v):
        """Returns the color name from HSV

        :return: Name of the color as a string
        :rtype: str
        """
        if h < 15:
            return "red"
        elif h < 30:
            return "orange"
        elif h < 75:
            return "yellow"
        elif h < 140:
            return "green"
        elif h < 260:
            return "blue"
        elif h < 330:
            return "magenta"
        else:
            return "red"
    
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
        hue, sat, val = self.get_color_hsv()
        return self.segment_color(hue, sat, val)

    def get_ambient_light(self):
        """Returns the ambient light

        :return: Ambient light
        :rtype: int
        """
        self._device.mode(2)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self._device.get(self._device.FORMAT_SI)[0])
        return int(sum(readings)/len(readings))
    
    def get_reflected_light(self):
        """Returns the reflected light

        :return: Reflected light
        :rtype: int
        """
        self._device.mode(1)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self._device.get(self._device.FORMAT_SI)[0])
        return int(sum(readings)/len(readings))

    def get_color_rgbi(self):
        """Returns the color 

        :return: RGBI representation 
        :rtype: tuple
        """
        self._device.mode(5)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self._device.get(self._device.FORMAT_SI))
        rgbi = []
        for i in range(4):
            rgbi.append(int(sum([rgbi[i] for rgbi in readings]) / len(readings)))
        return rgbi

    def get_color_hsv(self):
        """Returns the color 

        :return: HSV representation 
        :rtype: tuple
        """
        self._device.mode(6)
        readings = []
        for i in range(self.avg_reads):
            readings.append(self._device.get(self._device.FORMAT_SI))
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
        self._device.mode(5)
        lock = threading.Lock()
        
        def both(lst):
            r, g, b = lst[2:]
            r, g, b = int((r/1024)*255), int((g/1024)*255), int((b/1024)*255)
            h, s, v = self.rgb_to_hsv(r, g, b)
            seg = self.segment_color(h, s, v)
            if seg == color:
                lock.release()

        self._device.callback(both)
        lock.acquire()
        lock.acquire()
        self._device.callback(None)

    def wait_for_new_color(self):
        """Waits for new color or returns immediately if first call
        """
        self._device.mode(5)

        if self._old_color is None:
            self._old_color = self.get_color()
            return self._old_color

        lock = threading.Lock()
        
        def both(lst):
            r, g, b = lst[2:]
            r, g, b = int((r/1024)*255), int((g/1024)*255), int((b/1024)*255)
            h, s, v = self.rgb_to_hsv(r, g, b)
            seg = self.segment_color(h, s, v)
            if seg != self._old_color:
                self._old_color = seg
                lock.release()
        
        self._device.callback(both)
        lock.acquire()
        lock.acquire()
        self._device.callback(None)
        return self._old_color
