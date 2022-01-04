from .serinterface import BuildHAT
from .devices import Device
from .devicetypes import DeviceTypes
import os
import sys
import weakref

class Hat:
    """Allows enumeration of devices which are connected to the hat
    """
    def __init__(self, device="/dev/serial0"):
        if not Device._instance:
            data = os.path.join(os.path.dirname(sys.modules["buildhat"].__file__),"data/")
            firm = os.path.join(data,"firmware.bin")
            sig = os.path.join(data,"signature.bin")
            ver = os.path.join(data,"version")
            vfile = open(ver)
            v = int(vfile.read())
            vfile.close()
            Device._instance = BuildHAT(firm, sig, v, device=device)
            weakref.finalize(self, self._close)

    def get(self):
        """Gets devices which are connected or disconnected 

        :return: Dictionary of devices
        :rtype: dict
        """
        devices = {}
        for i in range(4):
            name = DeviceTypes.name_for_id(Device._instance.connections[i].typeid)
            if Device._instance.connections[i].typeid == -1:
                name = DeviceTypes._disconnected_device_name
            devices[chr(ord('A')+i)] = {"typeid" : Device._instance.connections[i].typeid,
                                        "connected" : Device._instance.connections[i].connected,
                                        "name" : name,
                                        "description": DeviceTypes.desc_for_id(Device._instance.connections[i].typeid)}
        return devices

    def _close(self):
        Device._instance.shutdown()
