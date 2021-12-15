from .devices import Device
from .exc import DeviceInvalid

class Light(Device):
    """Light

    Use on()/off() functions to turn lights on/off

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no light attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self.typeid != 8:
            raise DeviceInvalid('There is not a light connected to port %s (Found %s)' % (port, self.name))
