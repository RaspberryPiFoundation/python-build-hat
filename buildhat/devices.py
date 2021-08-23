from .serinterface import BuildHAT
import weakref
import time
import os
import sys
import gc

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
            data = os.path.join(os.path.dirname(sys.modules["buildhat"].__file__),"data/")
            firm = os.path.join(data,"firmware.bin")
            sig = os.path.join(data,"signature.bin")
            ver = os.path.join(data,"version")
            vfile = open(ver)
            v = int(vfile.read())
            vfile.close()
            Device._instance = BuildHAT(firm, sig, v)
            weakref.finalize(self, self.close)

    def _whatami(self, port):
        """Determine name of device on port

        :param port: Port of device
        """
        port = getattr(self._instance.port, port)
        if port.info()['type'] in self._device_names:
            return self._device_names[port.info()['type']]
        else:
            return "Unknown"

    def close(self):
        Device._instance.shutdown()

class PortDevice(Device):
    """Device which uses port"""
    def __init__(self, port):
        super().__init__()
        self._port = getattr(self._instance.port, port)
        self._device = self._port.device
