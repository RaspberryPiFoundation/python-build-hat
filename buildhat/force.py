"""Force sensor handling functionality"""

from threading import Condition

from .devices import Device


class ForceSensor(Device):
    """Force sensor

    :param port: Port of device
    :raises DeviceError: Occurs if there is no force sensor attached to port
    """

    def __init__(self, port, threshold_force=1):
        """Initialise force sensor

        :param port: Port of device
        :param threshold_force: Optional
        """
        super().__init__(port)
        self.mode([(0, 0), (1, 0), (3, 0)])
        self._when_pressed = None
        self._when_released = None
        self._fired_pressed = False
        self._fired_released = False
        self._cond_force = Condition()
        self._threshold_force = threshold_force

    def _intermediate(self, data):
        with self._cond_force:
            self._data = data[0]
            self._cond_force.notify()
        if data[0] >= self.threshold_force and not self._fired_pressed:
            if self._when_pressed is not None:
                self._when_pressed(data[0])
            self._fired_pressed = True
            self._fired_released = False
        if data[0] < self.threshold_force and not self._fired_released:
            if self._when_released is not None:
                self._when_released(data[0])
            self._fired_pressed = False
            self._fired_released = True

    @property
    def threshold_force(self):
        """Threshold force

        :getter: Returns threshold force
        :setter: Sets threshold force
        :return: Threshold force
        """
        return self._threshold_force

    @threshold_force.setter
    def threshold_force(self, value):
        self._threshold_force = value

    def get_force(self):
        """Return the force in (N)

        :return: The force exerted on the button
        :rtype: int
        """
        return self.get()[0]

    def get_peak_force(self):
        """Get the maximum force registered since the sensor was reset

        (The sensor gets reset when the firmware is reloaded)

        :return: 0 - 100
        :rtype: int
        """
        return self.get()[2]

    def is_pressed(self):
        """Get whether the button is pressed

        :return: If button is pressed
        :rtype: bool
        """
        return self.get()[1] == 1

    @property
    def when_pressed(self):
        """Handle force events

        :getter: Returns function to be called when pressed
        :setter: Sets function to be called when pressed
        :return: Callback function
        """
        return self._when_pressed

    @when_pressed.setter
    def when_pressed(self, value):
        """Call back, when button is has pressed

        :param value: Callback function
        """
        self._when_pressed = value
        self.callback(self._intermediate)

    @property
    def when_released(self):
        """Handle force events

        :getter: Returns function to be called when released
        :setter: Sets function to be called when released
        :return: Callback function
        """
        return self._when_pressed

    @when_released.setter
    def when_released(self, value):
        """Call back, when button is has released

        :param value: Callback function
        """
        self._when_released = value
        self.callback(self._intermediate)

    def wait_until_pressed(self, force=1):
        """Wait until the button is pressed

        :param force: Optional
        """
        self.callback(self._intermediate)
        with self._cond_force:
            self._cond_force.wait()
            while self._data < force:
                self._cond_force.wait()

    def wait_until_released(self, force=0):
        """Wait until the button is released

        :param force: Optional
        """
        self.callback(self._intermediate)
        with self._cond_force:
            self._cond_force.wait()
            while self._data > force:
                self._cond_force.wait()
