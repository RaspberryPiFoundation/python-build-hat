from .devices import Device
from .exc import DeviceInvalid, DistanceSensorException
from threading import Condition
import threading

class DistanceSensor(Device):
    """Distance sensor

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no distance sensor attached to port
    """
    def __init__(self, port, threshold_distance=100):
        super().__init__(port)
        if self.typeid != 62:
            raise DeviceInvalid('There is not a distance sensor connected to port %s (Found %s)' % (port, self.name))
        self.on()
        self.mode(0)
        self._cond_data = Condition()
        self._when_in_range = None
        self._when_out_of_range = None
        self._fired_in = False
        self._fired_out = False
        self._threshold_distance = threshold_distance
        self._distance = -1

    def _intermediate(self, data):
        self._distance = data[0]
        if self._distance != -1 and self._distance < self.threshold_distance and not self._fired_in:
            if self._when_in_range is not None:
                self._when_in_range(data[0])
            self._fired_in = True
            self._fired_out = False
        if self._distance != -1 and self._distance > self.threshold_distance and not self._fired_out:
            if self._when_out_of_range is not None:
                self._when_out_of_range(data[0])
            self._fired_in = False
            self._fired_out = True
        with self._cond_data:
            self._data = data[0]
            self._cond_data.notify()

    @property
    def distance(self):
        """
        :getter: Returns distance
        """
        return self._distance

    @property
    def threshold_distance(self):
        """
        :getter: Returns threshold distance
        :setter: Sets threshold distance
        """
        return self._threshold_distance

    @threshold_distance.setter
    def threshold_distance(self, value):
        self._threshold_distance = value

    def get_distance(self):
        """
        Returns the distance from ultrasonic sensor to object

        :return: Distance from ultrasonic sensor
        :rtype: int
        """
        dist = self.get()[0]
        return dist

    @property
    def when_in_range(self):
        """
        Handles motion events

        :getter: Returns function to be called when in range
        :setter: Sets function to be called when in range
        """
        return self._when_in_range

    @when_in_range.setter
    def when_in_range(self, value):
        """Calls back, when distance in range"""
        self._when_in_range = value
        self.callback(self._intermediate)

    @property
    def when_out_of_range(self):
        """
        Handles motion events

        :getter: Returns function to be called when out of range
        :setter: Sets function to be called when out of range
        """
        return self._when_out_of_range

    @when_out_of_range.setter
    def when_out_of_range(self, value):
        """Calls back, when distance out of range"""
        self._when_out_of_range = value
        self.callback(self._intermediate)

    def wait_for_out_of_range(self, distance):
        """Waits until object is farther than specified distance

        :param distance: Distance
        """
        self.callback(self._intermediate)
        with self._cond_data:
            self._cond_data.wait()
            while self._data < distance:
                self._cond_data.wait()
        
    def wait_for_in_range(self, distance):
        """Waits until object is closer than specified distance

        :param distance: Distance
        """
        self.callback(self._intermediate)
        with self._cond_data:
            self._cond_data.wait()
            while self._data == -1 or self._data > distance:
                self._cond_data.wait()

    def eyes(self, *args):
        """
        Brightness of LEDs on sensor
        (Sensor Right Upper, Sensor Left Upper, Sensor Right Lower, Sensor Left Lower)

        :param \*args: Four brightness arguments of 0 to 100
        """
        out = [0xc5]
        if len(args) != 4:
            raise DistanceSensorException("Need 4 brightness args, of 0 to 100")
        for v in args:
            if not (v >= 0 and v <= 100):
                raise DistanceSensorException("Need 4 brightness args, of 0 to 100")
            out += [v]
        self._write1(out)

    def on(self):
        """
        Turns on the sensor
        """
        self._write("port {} ; set -1\r".format(self.port))
