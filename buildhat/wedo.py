from .devices import Device
from .exc import DeviceInvalid

class TiltSensor(Device):
    """Tilt sensor

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no tilt sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self.typeid != 34:
            raise DeviceInvalid('There is not a tilt sensor connected to port %s (Found %s)' % (port, self.name))
        self.mode(0)

    def get_tilt(self):
        """
        Returns the tilt from tilt sensor to object

        :return: Tilt from tilt sensor
        :rtype: tuple
        """
        return tuple(self.get())

class MotionSensor(Device):
    """Motion sensor

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no motion sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self.typeid != 35:
            raise DeviceInvalid('There is not a motion sensor connected to port %s (Found %s)' % (port, self.name))
        self.mode(0)

    def get_distance(self):
        """
        Returns the distance from motion sensor to object

        :return: Distance from motion sensor
        :rtype: int
        """
        return self.get()[0]
