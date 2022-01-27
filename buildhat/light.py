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
