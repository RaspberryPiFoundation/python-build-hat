"""Light device handling functionality"""

from .devices import Device
from .exc import LightError


class Light(Device):
    """Light

    Use on()/off() functions to turn lights on/off

    :param port: Port of device
    :raises DeviceError: Occurs if there is no light attached to port
    """

    def __init__(self, port):
        """
        Initialise light

        :param port: Port of device
        """
        super().__init__(port)

    def _shutdown(self):
        self.off()

    def brightness(self, brightness):
        """
        Brightness of LEDs

        :param brightness: Brightness argument 0 to 100
        :raises LightError: Occurs if invalid brightness passed
        """
        if not (brightness >= 0 and brightness <= 100):
            raise LightError("Need brightness arg, of 0 to 100")
        self._write("port {} ; on ; plimit {}\r".format(self.port, brightness / 100.0))
