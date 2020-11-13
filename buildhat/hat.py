from build_hat import BuildHAT

"""
    Assign attributes from actual device object, to our wrapper.

    This functions better than simply overriding __getattr__ 
    method, as it allows autocompletion via the Python REPL.
"""


def patchattr(toobj, fromobj):
    for attribute in dir(fromobj):
        if not attribute.startswith("__") and getattr(fromobj, attribute):
            setattr(toobj, attribute, getattr(fromobj, attribute))


class Device:

    _instance = None

    def __init__(self):
        if not Device._instance:
            Device._instance = BuildHAT()


class Motor(Device):
    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._motor = self._port.motor
        patchattr(self, self._motor)


class MotorPair(Device):
    def __init__(self, motora, motorb):
        self._motora = motora
        self._motorb = motorb
        patchattr(self, motora.pair(motorb._motor))


class Force(Device):
    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._device = self._port.device
        patchattr(self, self._device)

    def get_force(self):
        return self._device.get()[2]

    def get_pressed(self):
        return self._device.get()[1] == 1


class Distance(Device):
    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._device = self._port.device
        patchattr(self, self._device)

    def get_distance(self):
        return self._device.get()[0]
