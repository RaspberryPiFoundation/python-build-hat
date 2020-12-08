from build_hat import BuildHAT
import time

class Device:
    """Creates a single instance of the buildhat for all devices to use"""
    _instance = None

    def __init__(self):
        if not Device._instance:
            Device._instance = BuildHAT()
            """FixMe - this is added so that we wait a little before
            initialising sensors etc. otherwise we can get 
            RuntimeError: There is no device attached to port B.

            See if there's a way to detect if hat is ready.
            """
            time.sleep(1)

class PortDevice(Device):
    """Device which uses port"""
    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._device = self._port.device
