from .devices import PortDevice 
import threading
import time

class Matrix(PortDevice):
    """LED Matrix

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no led matrix attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self._port.info()['type'] != 64:
            raise RuntimeError('There is not a led matrix connected to port %s (Found %s)' % (port, self.whatami(port)))
        self._typeid = 64
        self._device.on()
        self._device.mode(2)
    
    def write(self, pixels):
        self._device.select()
        self._device.write([0xc2]+[0xA0 | x for x in pixels])
        self._device.deselect()
