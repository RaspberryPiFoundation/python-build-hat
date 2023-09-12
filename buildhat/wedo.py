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

    default_mode = 0

    def __init__(self, port):
        """
        Initialise motion sensor

        :param port: Port of device
        """
        super().__init__(port)
        self.mode(self.default_mode)

    def set_default_data_mode(self, mode):
        """
        Set the mode most often queried from this device.

        This significantly improves performance when repeatedly accessing data

        :param mode: 0 for distance (default), 1 for movement count
        """
        if mode == 1 or mode == 0:
            self.default_mode = mode
            self.mode(mode)

    def get_distance(self):
        """
        Return the distance from motion sensor

        :return: Distance from motion sensor
        :rtype: int
        """
        return self._get_data_from_mode(0)

    def get_movement_count(self):
        """
        Return the movement counter

        This is the count of how many times the sensor has detected an object
        that moved within 4 blocks of the sensor since the sensor has been
        plugged in or the BuildHAT reset

        :return: Count of objects detected
        :rtype: int
        """
        return self._get_data_from_mode(1)

    def _get_data_from_mode(self, mode):
        if self.default_mode == mode:
            return self.get()[0]
        else:
            self.mode(mode)
            retval = self.get()[0]
            self.mode(self.default_mode)
            return retval
