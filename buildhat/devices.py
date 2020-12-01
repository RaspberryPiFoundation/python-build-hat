from build_hat import BuildHAT

def patchattr(toobj, fromobj):
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
        patchattr(self, self._device)



