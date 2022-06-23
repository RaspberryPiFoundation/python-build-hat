"""HAT handling functionality"""

from .devices import Device


class Hat:
    """Allows enumeration of devices which are connected to the hat"""

    def __init__(self, device=None, debug=False):
        """Hat

        :param device: Optional string containing path to Build HAT serial device
        :param debug: Optional boolean to log debug information
        """
        self.led_status = -1
        if device is None:
            Device._setup(debug=debug)
        else:
            Device._setup(device=device, debug=debug)

    def get(self):
        """Get devices which are connected or disconnected

        :return: Dictionary of devices
        :rtype: dict
        """
        devices = {}
        for i in range(4):
            name = Device.UNKNOWN_DEVICE
            if Device._instance.connections[i].typeid in Device._device_names:
                name = Device._device_names[Device._instance.connections[i].typeid][0]
                desc = Device._device_names[Device._instance.connections[i].typeid][1]
            elif Device._instance.connections[i].typeid == -1:
                name = Device.DISCONNECTED_DEVICE
                desc = ''
            devices[chr(ord('A') + i)] = {"typeid": Device._instance.connections[i].typeid,
                                          "connected": Device._instance.connections[i].connected,
                                          "name": name,
                                          "description": desc}
        return devices

    def get_vin(self):
        """Get the voltage present on the input power jack

        :return: Voltage on the input power jack
        :rtype: float
        """
        Device._instance.write(b"vin\r")
        with Device._instance.vincond:
            Device._instance.vincond.wait()

        return Device._instance.vin

    def _set_led(self, intmode):
        if isinstance(intmode, int) and intmode >= -1 and intmode <= 3:
            self.led_status = intmode
            Device._instance.write(f"ledmode {intmode}\r".encode())

    def set_leds(self, color="voltage"):
        """Set the two LEDs on or off on the BuildHAT.

        By default the color depends on the input voltage with green being nominal at around 8V
        (The fastest time the LEDs can be perceptually toggled is around 0.025 seconds)

        :param color: orange, green, both, off, or voltage (default)
        """
        if color == "orange":
            self._set_led(1)
        elif color == "green":
            self._set_led(2)
        elif color == "both":
            self._set_led(3)
        elif color == "off":
            self._set_led(0)
        elif color == "voltage":
            self._set_led(-1)
        else:
            return

    def orange_led(self, status=True):
        """Turn the BuildHAT's orange LED on or off

        :param status: True to turn it on, False to turn it off
        """
        if status:
            if self.led_status == 3 or self.led_status == 1:
                # already on
                return
            elif self.led_status == 2:
                self._set_led(3)
            # off or default
            else:
                self._set_led(1)
        else:
            if self.led_status == 1 or self.led_status == -1:
                self._set_led(0)
            elif self.led_status == 3:
                self._set_led(2)

    def green_led(self, status=True):
        """Turn the BuildHAT's green LED on or off

        :param status: True to turn it on, False to turn it off
        """
        if status:
            if self.led_status == 3 or self.led_status == 2:
                # already on
                return
            elif self.led_status == 1:
                self._set_led(3)
            # off or default
            else:
                self._set_led(2)
        else:
            if self.led_status == 2 or self.led_status == -1:
                self._set_led(0)
            elif self.led_status == 3:
                self._set_led(1)

    def _close(self):
        Device._instance.shutdown()
