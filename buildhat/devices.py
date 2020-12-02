from build_hat import BuildHAT

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
