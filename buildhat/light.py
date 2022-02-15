from .devices import Device
from .exc import DeviceInvalid, LightException

class Light(Device):
    """Light

    Use on()/off() functions to turn lights on/off

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no light attached to port
    """
    def __init__(self, port):
        super().__init__(port)

    def brightness(self, brightness):
        """
        Brightness of LEDs

        :param brightness: Brightness argument 0 to 100
        """
        if not (brightness >= 0 and brightness <= 100) :
            raise LightException("Need brightness arg, of 0 to 100")
        self._write("port {} ; on ; plimit {}\r".format(self.port, brightness / 100.0))
