from .devices import PortDevice 
import threading
import time

class MatrixInvalidPixel(Exception):
    pass

class Matrix(PortDevice):
    """LED Matrix

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no led matrix attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self._port.info()['type'] != 64:
            raise RuntimeError('There is not a led matrix connected to port %s (Found %s)' % (port, self._whatami(port)))
        self._typeid = 64
        self._device.on()
        self._device.mode(2)
    
    def write(self, pixels):
        """Write pixel data to LED matrix

        :param pixels: Array of 9 tuples, with colour (0-9) and brightness (0-10) (see example for more detail)
        """
        self._device.select()
        pix = []
        for colour, brightness in pixels:
            if not (brightness >= 0 and brightness <= 10):
                raise MatrixInvalidPixel("Invalid brightness specified")
            if not (colour >= 0 and colour <= 9):
                raise MatrixInvalidPixel("Invalid pixel specified")
            pix.append((brightness << 4) | colour)
        self._device.write([0xc2]+pix)
        self._device.deselect()
