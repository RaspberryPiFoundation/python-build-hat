from build_hat import BuildHAT
import time

class Device:
    """Creates a single instance of the buildhat for all devices to use"""
    _instance = None

    _device_names = { 61: "ColorSensor",
                      62: "DistanceSensor",
                      63: "ForceSensor",
                      38: "Motor",
                      46: "Motor",
                      47: "Motor",
                      48: "Motor",
                      49: "Motor",
                      65: "Motor",
                      75: "Motor",
                      76: "Motor"
                    }
    
    def __init__(self):
        if not Device._instance:
            Device._instance = BuildHAT()
            """FixMe - this is added so that we wait a little before
            initialising sensors etc. otherwise we can get 
            RuntimeError: There is no device attached to port B.

            See if there's a way to detect if hat is ready.
            """
            time.sleep(7)

    def whatami(self, port):
        """Determine name of device on port

        :param port: Port of device
        """
        port = getattr(self._instance.port, port)
        if port.info()['type'] in self._device_names:
            return self._device_names[port.info()['type']]
        else:
            return "Unknown"

class PortDevice(Device):
    """Device which uses port"""
    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._device = self._port.device
