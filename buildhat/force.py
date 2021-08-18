from .devices import PortDevice 
import threading

class ForceSensor(PortDevice):
    """Force sensor

    :param port: Port of device
    :raises RuntimeError: Occurs if there is no force sensor attached to port
    """
    def __init__(self, port):
        super().__init__(port)
        if self._port.info()['type'] != 63:
            raise RuntimeError('There is not a force sensor connected to port %s (Found %s)' % (port, self.whatami(port)))
        self._device.mode([(0,0),(1,0)])
        self._when_force = None

    def get_force_percentage(self):
        """Returns the force in percentage

        :return: The force exherted on the button
        :rtype: int
        """
        return self._device.get(self._device.FORMAT_PCT)[0]

    def get_force_newton(self):
        """Returns the force in newtons

        :return: The force exherted on the button
        :rtype: int
        """
        return self._device.get(self._device.FORMAT_SI)[0]

    def is_pressed(self):
        """Gets whether the button is pressed

        :return: If button is pressed
        :rtype: bool
        """
        return self._device.get()[1] == 1

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

        self._when_force = lambda lst: value(lst[0],lst[1])
        self._device.callback(self._when_force)

    def wait_until_pressed(self):
        """Waits until the button is pressed
        """
        oldcall = self._when_force
        lock = threading.Lock()
        
        def both(lst):
            if lst[1] == 1:
                lock.release()
            if oldcall is not None:
                oldcall(lst)

        self._device.callback(both)
        lock.acquire()
        lock.acquire()

        self._device.callback(oldcall)

    def wait_until_released(self):
        """Waits until the button is released
        """
        oldcall = self._when_force
        lock = threading.Lock()
        
        def both(lst):
            if lst[1] == 0:
                lock.release()
            if oldcall is not None:
                oldcall(lst)

        self._device.callback(both)
        lock.acquire()
        lock.acquire()

        self._device.callback(oldcall)
