from build_hat import BuildHAT


def _patchattr(toobj, fromobj):
    """
    Assign attributes from actual device object, to our wrapper.
    This functions better than simply overriding __getattr__
    method, as it allows autocompletion via the Python REPL.
    """
    for attribute in dir(fromobj):
        if not attribute.startswith("__") and getattr(fromobj, attribute):
            setattr(toobj, attribute, getattr(fromobj, attribute))


class Device:
    """Creates a single instance of the buildhat for all devices to use"""

    _instance = None

    def __init__(self):
        if not Device._instance:
            Device._instance = BuildHAT()


class PortDevice(Device):
    """Device which uses port"""

    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._device = self._port.device
        _patchattr(self, self._device)

class Motor(PortDevice):
    """Motor device

    :param port: Port of device
    """

    def __init__(self, port):
        super().__init__(port)
        self._motor = self._port.motor
        _patchattr(self, self._motor)


class MotorPair(Device):
    """Pair of motors

    :param motora: One of the motors to drive
    :param motorb: Other motor in pair to drive
    """
    
    def __init__(self, motora, motorb):
        self._motora = motora
        self._motorb = motorb
        _patchattr(self, motora.pair(motorb._motor))


class ForceSensor(PortDevice):
    """Force sensor

    :param port: Port of device
    """

    def __init__(self, port):
        super().__init__(port)

    def get_force_percentage(self):
        """
        Returns the force in percentage

        :return: The force exherted on the button
        :rtype: int
        """
        return self._device.get(self._device.FORMAT_PCT)[2]

    def get_force_newton(self):
        """
        Returns the force in newtons

        :return: The force exherted on the button
        :rtype: int
        """
        return self._device.get(self._device.FORMAT_SI)[2]

    def is_pressed(self):
        """
        Gets whether the button is pressed

        :return: If button is pressed
        :rtype: bool
        """
        return self._device.get()[1] == 1


class DistanceSensor(PortDevice):
    """Distance sensor

    :param port: Port of device
    """

    def __init__(self, port):
        super().__init__(port)

    def get_distance_cm(self):
        """
        Returns the distance from ultrasonic sensor to object in cm

        :return: Distance from ultrasonic sensor
        :rtype: float
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
        :rtype: float
        """
        return self._device.get(self._device.FORMAT_PCT)[0]

