from .devices import Device
from .exc import DeviceInvalid
from threading import Condition
import threading

class ForceSensor(Device):
    """Force sensor

    :param port: Port of device
    :raises DeviceInvalid: Occurs if there is no force sensor attached to port
    """
    def __init__(self, port, threshold_force=1):
        super().__init__(port)
        if self.typeid != 63:
            raise DeviceInvalid('There is not a force sensor connected to port %s (Found %s)' % (port, self.name))
        self.mode([(0,0),(1,0)])
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
        """
        :getter: Returns threshold force
        :setter: Sets threshold force
        """
        return self._threshold_force

    @threshold_force.setter
    def threshold_force(self, value):
        self._threshold_force = value

    def get_force(self):
        """Returns the force in (N)

        :return: The force exerted on the button
        :rtype: int
        """
        return self.get()[0]

    def is_pressed(self):
        """Gets whether the button is pressed

        :return: If button is pressed
        :rtype: bool
        """
        return self.get()[1] == 1

    @property
    def when_pressed(self):
        """Handles force events

        :getter: Returns function to be called when pressed
        :setter: Sets function to be called when pressed
        """
        return self._when_pressed

    @when_pressed.setter
    def when_pressed(self, value):
        """Calls back, when button is has pressed"""
        self._when_pressed = value
        self.callback(self._intermediate)

    @property
    def when_released(self):
        """Handles force events

        :getter: Returns function to be called when released
        :setter: Sets function to be called when released
        """
        return self._when_pressed

    @when_released.setter
    def when_released(self, value):
        """Calls back, when button is has released"""
        self._when_released = value
        self.callback(self._intermediate)

    def wait_until_pressed(self, force=1):
        """Waits until the button is pressed
        """
        self.callback(self._intermediate)
        with self._cond_force:
            self._cond_force.wait()
            while self._data < force:
                self._cond_force.wait()

    def wait_until_released(self, force=0):
        """Waits until the button is released
        """
        self.callback(self._intermediate)
        with self._cond_force:
            self._cond_force.wait()
            while self._data > force:
                self._cond_force.wait()
