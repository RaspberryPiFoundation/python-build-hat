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
        self.default_speed = 100

    def set_default_speed(self, default_speed):
        """Sets the default speed of the motor

        :param default_speed: Speed ranging from -100 to 100
        """

        self.default_speed = default_speed

    def run_for_rotations(self, rotations, speed=None):
        """Runs motor for N rotations

        :param rotations: Number of rotations
        :param speed: Speed ranging from -100 to 100
        """

        if speed is None:
            self._motor.run_for_rotations(rotations, self.default_speed)
        else:
            self._motor.run_for_rotations(rotations, speed)

    def run_for_degrees(self, degrees, speed=None):
        """Runs motor for N degrees

        :param degrees: Number of degrees to rotate
        :param speed: Speed ranging from -100 to 100
        """

        if speed is None:
            self._motor.run_for_degrees(degrees, self.default_speed)
        else:
            self._motor.run_for_degrees(degrees, speed)

    def run_to_position(self, degrees, speed=None):
        """Runs motor to position (in degrees)

        :param degrees: Position in degrees
        :param speed: Speed ranging from -100 to 100
        """

        if speed is None:
            self._motor.run_to_position(degrees, self.default_speed)
        else:
            self._motor.run_to_position(degrees, speed)

    def run_for_seconds(self, seconds, speed=None):
        """Runs motor for N seconds

        :param seconds: Time in seconds
        :param speed: Speed ranging from -100 to 100
        """

        if speed is None:
            self._motor.run_for_time(int(seconds * 1000), self.default_speed)
        else:
            self._motor.run_for_time(int(seconds * 1000), speed)

    def start(self, speed=None):
        """Start motor

        :param speed: Speed ranging from -100 to 100
        """

        if speed is None:
            self._motor.run_at_speed(self.default_speed)
        else:
            self._motor.run_at_speed(speed)

    def stop(self):
        """Stops motor"""
        self._motor.brake()

    def get_position(self):
        """Gets position of motor with relation to preset position (can be negative or positive).

        :return: Position of motor
        :rtype: int
        """

        return self._motor.get()[2]

    def get_speed(self):
        """Gets speed of motor

        :return: Speed of motor
        :rtype: int
        """

        return self._device.get()[0]


class MotorPair(Device):
    """Pair of motors

    :param motora: One of the motors to drive
    :param motorb: Other motor in pair to drive
    """

    def __init__(self, leftport, rightport):
        super().__init__()
        self._leftport = getattr(self._instance.port, leftport)
        self._rightport = getattr(self._instance.port, rightport)
        self._leftmotor = self._leftport.motor
        self._rightmotor = self._rightport.motor
        self._pair = self._leftmotor.pair(self._rightmotor)
        self.default_speed = 100

    def set_default_speed(self, default_speed):
        """Sets the default speed of the motor

        :param default_speed: Speed ranging from -100 to 100
        """

        self.default_speed = default_speed

    def run_for_rotations(self, rotations, speedl=None, speedr=None):
        """Runs pair of motors for N rotations

        :param rotations: Number of rotations
        :param speedl: Speed ranging from -100 to 100
        :param speedr: Speed ranging from -100 to 100
        """

        if speedl is None and speedr is None:
            self._pair.run_for_degrees(int(rotations * 360), self.default_speed, self.default_speed)
        else:
            self._pair.run_for_degrees(int(rotations * 360), speedl, speedr)

    def run_for_degrees(self, degrees, speedl=None, speedr=None):
        """Runs pair of motors for degrees

        :param degrees: Number of degrees
        :param speedl: Speed ranging from -100 to 100
        :param speedr: Speed ranging from -100 to 100
        """

        if speedl is None and speedr is None:
            self._pair.run_for_degrees(degrees, self.default_speed, self.default_speed)
        else:
            self._pair.run_for_degrees(degrees, speedl, speedr)

    def run_for_seconds(self, seconds, speedl=None, speedr=None):
        """Runs pair for N seconds

        :param seconds: Time in seconds
        :param speedl: Speed ranging from -100 to 100
        :param speedr: Speed ranging from -100 to 100
        """

        if speedl is None and speedr is None:
            self._pair.run_for_time(int(seconds * 1000), self.default_speed, self.default_speed)
        else:
            self._pair.run_for_time(int(seconds * 1000), speedl, speedr)


    #def run_to_position(self, degreesl, degreesr, speed=None):
    #    """Runs pair to position (in degrees)
    #
    #    :param degreesl: Position in degrees for left motor
    #    :param degreesr: Position in degrees for right motor
    #    :param speed: Speed ranging from -100 to 100
    #    """
    #
    #    if speed is None:
    #        self._pair.run_to_position(degreesl, degreesr, self.default_speed)
    #    else:
    #        self._pair.run_to_position(degreesl, degreesr, speed)

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
