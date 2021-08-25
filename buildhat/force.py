from .devices import PortDevice
from threading import Condition
import threading

class ForceSensor(PortDevice):
    """Force sensor

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no force sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self._port.info()['type'] != 63:
            raise RuntimeError('There is not a force sensor connected to port %s (Found %s)' % (port, self._whatami(port)))
        self._typeid = 63
        self._device.mode([(0,0),(1,0)])
        self._callback = self.callback
        self._when_force = None
        self._when_pressed = None
        self._when_released = None
        self._fired_pressed = False
        self._fired_released = False
        self._cond_pressed = Condition()
        self._cond_released = Condition()

    def callback(self, data):
        if self._when_force is not None:
            self._when_force(data[0], data[1])
        if data[1] == 1:
            with self._cond_pressed:
                self._cond_pressed.notify()
        else:
            with self._cond_released:
                self._cond_released.notify()
        if self._when_pressed is not None:
            if data[1] == 1 and not self._fired_pressed:
                self._when_pressed()
                self._fired_pressed = True
                self._fired_released = False
        if self._when_released is not None:
            if data[1] == 0 and not self._fired_released:
                self._when_released()
                self._fired_pressed = False
                self._fired_released = True

    def get_force(self):
        """Returns the force in newtons

        :return: The force exherted on the button
        :rtype: int
        """
        return self._device.get(self._typeid)[0]

    def is_pressed(self):
        """Gets whether the button is pressed

        :return: If button is pressed
        :rtype: bool
        """
        return self._device.get(self._typeid)[1] == 1

    @property
    def when_force(self):
        """Handles force events

        :getter: Returns function to be called when force
        :setter: Sets function to be called when force
        """
        return self._when_force

    @when_force.setter
    def when_force(self, value):
        """Calls back, when force has changed"""
        self._when_force = value
        self._device.callback(self._callback)

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
        self._device.callback(self._callback)

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
        self._device.callback(self._callback)

    def wait_until_pressed(self):
        """Waits until the button is pressed
        """
        self._device.callback(self._callback)
        with self._cond_pressed:
            self._cond_pressed.wait()

    def wait_until_released(self):
        """Waits until the button is released
        """
        self._device.callback(self._callback)
        with self._cond_released:
            self._cond_released.wait()
