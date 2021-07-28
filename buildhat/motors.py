from .devices import PortDevice, Device
import threading

# See hub-python-module/drivers/m_sched_shortcake.h
MOTOR_SET = set([38, 46, 47, 48, 49, 65, 75, 76])

class Motor(PortDevice):
    """Motor device

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no motor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self._port.info()['type'] not in MOTOR_SET:
            raise RuntimeError('There is not a motor connected to port %s (Found %s)' % (port, self.whatami(port)))
        self._motor = self._port.motor
        self.default_speed = 50
        self._device.mode([(1,0),(2,0),(3,0)])
        self._when_rotated = None
        self._ramp = 0

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
            self.run_for_degrees(int(rotations * 360), self.default_speed)
        else:
            self.run_for_degrees(int(rotations * 360), speed)

    def run_for_degrees(self, degrees, speed=None):
        """Runs motor for N degrees

        :param degrees: Number of degrees to rotate
        :param speed: Speed ranging from -100 to 100
        """
        newpos = (degrees / 360.0) + self._ramp
        if speed is None:
            self._motor.run_for_degrees(newpos, self._ramp, self.default_speed)
        else:
            self._motor.run_for_degrees(newpos, self._ramp, speed)
        self._ramp = newpos

    def run_to_position(self, degrees, speed=None):
        """Runs motor to position (in degrees)

        :param degrees: Position in degrees
        :param speed: Speed ranging from -100 to 100
        """
        if speed is None:
            self._motor.run_to_position(degrees, self.default_speed)
        else:
            self._motor.run_to_position(degrees, speed)

    def run_for_seconds(self, seconds, speed=None, blocking=True):
        """Runs motor for N seconds

        :param seconds: Time in seconds
        :param speed: Speed ranging from -100 to 100
        """
        if speed is None:
            self._motor.run_for_time(int(seconds * 1000), self.default_speed, blocking=blocking)
        else:
            self._motor.run_for_time(int(seconds * 1000), speed, blocking=blocking)

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
        self._motor.float()

    def get_position(self):
        """Gets position of motor with relation to preset position (can be negative or positive).

        :return: Position of motor
        :rtype: int
        """
        return self._motor.get()[1]

    def get_aposition(self):
        """Gets absolute position of motor

        :return: Absolute position of motor
        :rtype: int
        """
        return self._motor.get()[2]

    def get_speed(self):
        """Gets speed of motor

        :return: Speed of motor
        :rtype: int
        """
        return self._device.get()[0]

    @property
    def when_rotated(self):
        """
        Handles rotatation events

        :getter: Returns function to be called when rotated
        :setter: Sets function to be called when rotated
        """
        return self._when_rotated

    @when_rotated.setter
    def when_rotated(self, value):
        """Calls back, when motor has been rotated"""
        self._when_rotated = lambda lst: value(lst[0], lst[1], lst[2])
        self._device.callback(self._when_rotated)


class MotorPair(Device):
    """Pair of motors

    :param motora: One of the motors to drive
    :param motorb: Other motor in pair to drive
    :raises RuntimeError: Occurs if there is no motor attached to port
    """
    def __init__(self, leftport, rightport):
        super().__init__()
        self._leftmotor = Motor(leftport)
        self._rightmotor = Motor(rightport)
        self.default_speed = 50

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
        if speedl is None:
            speedl = self.default_speed
        if speedr is None:
            speedr = self.default_speed
        self._leftmotor.run_for_degrees(int(rotations * 360), speedl)
        self._rightmotor.run_for_degrees(int(rotations * 360), speedr)

    def run_for_degrees(self, degrees, speedl=None, speedr=None):
        """Runs pair of motors for degrees

        :param degrees: Number of degrees
        :param speedl: Speed ranging from -100 to 100
        :param speedr: Speed ranging from -100 to 100
        """
        if speedl is None:
            speedl = self.default_speed
        if speedr is None:
            speedr = self.default_speed
        self._leftmotor.run_for_degrees(degrees, speedl)
        self._rightmotor.run_for_degrees(degrees, speedr)

    def run_for_seconds(self, seconds, speedl=None, speedr=None):
        """Runs pair for N seconds

        :param seconds: Time in seconds
        :param speedl: Speed ranging from -100 to 100
        :param speedr: Speed ranging from -100 to 100
        """
        if speedl is None:
            speedl = self.default_speed
        if speedr is None:
            speedr = self.default_speed
        self._leftmotor.run_for_seconds(seconds, speedl, blocking=False)
        self._rightmotor.run_for_seconds(seconds, speedr)

    def start(self, speedl=None, speedr=None):
        """Start motors

        :param speed: Speed ranging from -100 to 100
        """
        if speedl is None:
            speedl = self.default_speed
        if speedr is None:
            speedr = self.default_speed
        self._leftmotor.start(speedl)
        self._rightmotor.start(speedr)

    def stop(self):
        """Stop motors"""
        self._leftmotor.stop()
        self._rightmotor.stop()

    def run_to_position(self, degreesl, degreesr, speed=None):
        """Runs pair to position (in degrees)

           :param degreesl: Position in degrees for left motor
           :param degreesr: Position in degrees for right motor
           :param speed: Speed ranging from -100 to 100
        """
        if speed is None:
            self._leftmotor.run_to_position(degreesl, self.default_speed)
            self._rightmotor.run_to_position(degreesr, self.default_speed)
        else:
            self._leftmotor.run_to_position(degreesl, speed)
            self._rightmotor.run_to_position(degreesr, speed)
