from .devices import PortDevice
import threading

class DistanceSensor(PortDevice):
    """Distance sensor

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no distance sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self._port.info()['type'] != 62:
            raise RuntimeError('There is not a distance sensor connected to port %s (Found %s)' % (port, self.whatami(port)))
        self._device.mode(0)
        self._when_motion = None

    def get_distance_cm(self):
        """
        Returns the distance from ultrasonic sensor to object in cm

        :return: Distance from ultrasonic sensor
        :rtype: int
        """
        return self._device.get(self._device.FORMAT_SI)[0]

    def get_distance_inches(self):
        """
        Returns the distance from ultrasonic sensor to object in inches

        :return: Distance from ultrasonic sensor
        :rtype: float
        """
        return self._device.get(self._device.FORMAT_SI)[0] * 0.393701

    def get_distance_percentage(self):
        """
        Returns the distance from ultrasonic sensor to object in percentage

        :return: Distance from ultrasonic sensor
        :rtype: int
        """
        return self._device.get(self._device.FORMAT_PCT)[0]

    @property
    def when_motion(self):
        """
        Handles motion events

        :getter: Returns function to be called when movement
        :setter: Sets function to be called when movement
        """
        return self._when_motion

    @when_motion.setter
    def when_motion(self, value):
        """Calls back, when distance has changed"""
        self._when_motion = lambda lst: value(lst[0])
        self._device.callback(self._when_motion)

    def wait_until_distance_farther_cm(self, distance):
        """Waits until distance is farther than specified distance

        :param distance: Distance in cm
        """
        oldcall = self._when_motion
        lock = threading.Lock()
        
        def both(lst):
            if lst[0] > distance:
                lock.release()
            oldcall(lst)

        self._device.callback(both)
        lock.acquire()
        lock.acquire()

        self._device.callback(oldcall)

    def wait_until_distance_closer_cm(self, distance):
        """Waits until distance is closer than specified distance

        :param distance: Distance in cm
        """
        oldcall = self._when_motion
        lock = threading.Lock()
        
        def both(lst):
            if lst[0] != 0 and lst[0] < distance:
                lock.release()
            oldcall(lst)

        self._device.callback(both)
        lock.acquire()
        lock.acquire()

        self._device.callback(oldcall)
