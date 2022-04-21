"""WeDo sensor handling functionality"""

from .devices import Device


class TiltSensor(Device):
    """Tilt sensor

    :param port: Port of device
    :raises DeviceError: Occurs if there is no tilt sensor attached to port
    """

    def __init__(self, port):
        """
        Initialise tilt sensor

        :param port: Port of device
        """
        super().__init__(port)
        self.mode(0)

    def get_tilt(self):
        """
        Return the tilt from tilt sensor

        :return: Tilt from tilt sensor
        :rtype: tuple
        """
        return tuple(self.get())


class MotionSensor(Device):
    """Motion sensor

    :param port: Port of device
    :raises DeviceError: Occurs if there is no motion sensor attached to port
    """

    def __init__(self, port):
        """
        Initialise motion sensor

        :param port: Port of device
        """
        super().__init__(port)
        self.mode(0)

    def get_distance(self):
        """
        Return the distance from motion sensor

        :return: Distance from motion sensor
        :rtype: int
        """
        return self.get()[0]
