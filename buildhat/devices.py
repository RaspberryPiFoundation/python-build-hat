"""Functionality for handling Build HAT devices"""

import os
import sys
import weakref
from concurrent.futures import Future

from .exc import DeviceError
from .serinterface import BuildHAT


class Device:
    """Creates a single instance of the buildhat for all devices to use"""

    _instance = None
    _started = 0
    _device_names = {1: ("PassiveMotor", "PassiveMotor"),
                     2: ("PassiveMotor", "PassiveMotor"),
                     8: ("Light", "Light"),                                   # 88005
                     34: ("TiltSensor", "WeDo 2.0 Tilt Sensor"),              # 45305
                     35: ("MotionSensor", "MotionSensor"),                    # 45304
                     37: ("ColorDistanceSensor", "Color & Distance Sensor"),  # 88007
                     61: ("ColorSensor", "Color Sensor"),                     # 45605
                     62: ("DistanceSensor", "Distance Sensor"),               # 45604
                     63: ("ForceSensor", "Force Sensor"),                     # 45606
                     64: ("Matrix", "3x3 Color Light Matrix"),                # 45608
                     38: ("Motor", "Medium Linear Motor"),                    # 88008
                     46: ("Motor", "Large Motor"),                            # 88013
                     47: ("Motor", "XL Motor"),                               # 88014
                     48: ("Motor", "Medium Angular Motor (Cyan)"),            # 45603
                     49: ("Motor", "Large Angular Motor (Cyan)"),             # 45602
                     65: ("Motor", "Small Angular Motor"),                    # 45607
                     75: ("Motor", "Medium Angular Motor (Grey)"),            # 88018
                     76: ("Motor", "Large Angular Motor (Grey)")}             # 88017

    _used = {0: False,
             1: False,
             2: False,
             3: False}

    UNKNOWN_DEVICE = "Unknown"
    DISCONNECTED_DEVICE = "Disconnected"

    def __init__(self, port):
        """Initialise device

        :param port: Port of device
        :raises DeviceError: Occurs if incorrect port specified or port already used
        """
        if not isinstance(port, str) or len(port) != 1:
            raise DeviceError("Invalid port")
        p = ord(port) - ord('A')
        if not (p >= 0 and p <= 3):
            raise DeviceError("Invalid port")
        if Device._used[p]:
            raise DeviceError("Port already used")
        self.port = p
        Device._setup()
        self._simplemode = -1
        self._combimode = -1
        self._modestr = ""
        self._typeid = self._conn.typeid
        self._interval = 10
        if (
            self._typeid in Device._device_names
            and Device._device_names[self._typeid][0] != type(self).__name__  # noqa: W503
        ) or self._typeid == -1:
            raise DeviceError(f'There is not a {type(self).__name__} connected to port {port} (Found {self.name})')
        Device._used[p] = True

    @staticmethod
    def _setup(**kwargs):
        if Device._instance:
            return
        if (
            os.path.isdir(os.path.join(os.getcwd(), "data/"))
            and os.path.isfile(os.path.join(os.getcwd(), "data", "firmware.bin"))
            and os.path.isfile(os.path.join(os.getcwd(), "data", "signature.bin"))
            and os.path.isfile(os.path.join(os.getcwd(), "data", "version"))
        ):
            data = os.path.join(os.getcwd(), "data/")
        else:
            data = os.path.join(os.path.dirname(sys.modules["buildhat"].__file__), "data/")
        firm = os.path.join(data, "firmware.bin")
        sig = os.path.join(data, "signature.bin")
        ver = os.path.join(data, "version")
        vfile = open(ver)
        v = int(vfile.read())
        vfile.close()
        Device._instance = BuildHAT(firm, sig, v, **kwargs)
        weakref.finalize(Device._instance, Device._instance.shutdown)

    def __del__(self):
        """Handle deletion of device"""
        if hasattr(self, "port") and Device._used[self.port]:
            Device._used[self.port] = False
            self._conn.callit = None
            self.deselect()
            self.off()

    @staticmethod
    def name_for_id(typeid):
        """Translate integer type id to device name (python class)

        :param typeid: Type of device
        :return: Name of device
        """
        if typeid in Device._device_names:
            return Device._device_names[typeid][0]
        else:
            return Device.UNKNOWN_DEVICE

    @staticmethod
    def desc_for_id(typeid):
        """Translate integer type id to something more descriptive than the device name

        :param typeid: Type of device
        :return: Description of device
        """
        if typeid in Device._device_names:
            return Device._device_names[typeid][1]
        else:
            return Device.UNKNOWN_DEVICE

    @property
    def _conn(self):
        return Device._instance.connections[self.port]

    @property
    def connected(self):
        """Whether device is connected or not

        :return: Connection status
        """
        return self._conn.connected

    @property
    def typeid(self):
        """Type ID of device

        :return: Type ID
        """
        return self._typeid

    @property
    def typeidcur(self):
        """Type ID currently present

        :return: Type ID
        """
        return self._conn.typeid

    @property
    def _hat(self):
        """Hat instance

        :return: Hat instance
        """
        return Device._instance

    @property
    def name(self):
        """Determine name of device on port

        :return: Device name
        """
        if not self.connected:
            return Device.DISCONNECTED_DEVICE
        elif self.typeidcur in self._device_names:
            return self._device_names[self.typeidcur][0]
        else:
            return Device.UNKNOWN_DEVICE

    @property
    def description(self):
        """Device on port info

        :return: Device description
        """
        if not self.connected:
            return Device.DISCONNECTED_DEVICE
        elif self.typeidcur in self._device_names:
            return self._device_names[self.typeidcur][1]
        else:
            return Device.UNKNOWN_DEVICE

    def isconnected(self):
        """Whether it is connected or not

        :raises DeviceError: Occurs if device no longer the same
        """
        if not self.connected:
            raise DeviceError("No device found")
        if self.typeid != self.typeidcur:
            raise DeviceError("Device has changed")

    def reverse(self):
        """Reverse polarity"""
        self._write(f"port {self.port} ; port_plimit 1 ; set -1\r")

    def get(self):
        """Extract information from device

        :return: Data from device
        :raises DeviceError: Occurs if device not in valid mode
        """
        self.isconnected()
        if self._simplemode == -1 and self._combimode == -1:
            raise DeviceError("Not in simple or combimode")
        ftr = Future()
        self._hat.portftr[self.port].append(ftr)
        return ftr.result()

    def mode(self, modev):
        """Set combimode or simple mode

        :param modev: List of tuples for a combimode, or integer for simple mode
        """
        self.isconnected()
        if isinstance(modev, list):
            modestr = ""
            for t in modev:
                modestr += f"{t[0]} {t[1]} "
            if self._simplemode == -1 and self._combimode == 0 and self._modestr == modestr:
                return
            self._write(f"port {self.port}; select\r")
            self._combimode = 0
            self._write((f"port {self.port} ; combi {self._combimode} {modestr} ; "
                         f"select {self._combimode} ; "
                         f"selrate {self._interval}\r"))
            self._simplemode = -1
            self._modestr = modestr
            self._conn.combimode = 0
            self._conn.simplemode = -1
        else:
            if self._combimode == -1 and self._simplemode == int(modev):
                return
            # Remove combi mode
            if self._combimode != -1:
                self._write(f"port {self.port} ; combi {self._combimode}\r")
            self._write(f"port {self.port}; select\r")
            self._combimode = -1
            self._simplemode = int(modev)
            self._write(f"port {self.port} ; select {int(modev)} ; selrate {self._interval}\r")
            self._conn.combimode = -1
            self._conn.simplemode = int(modev)

    def select(self):
        """Request data from mode

        :raises DeviceError: Occurs if device not in valid mode
        """
        self.isconnected()
        if self._simplemode != -1:
            idx = self._simplemode
        elif self._combimode != -1:
            idx = self._combimode
        else:
            raise DeviceError("Not in simple or combimode")
        self._write(f"port {self.port} ; select {idx} ; selrate {self._interval}\r")

    def on(self):
        """Turn on sensor"""
        self._write(f"port {self.port} ; port_plimit 1 ; on\r")

    def off(self):
        """Turn off sensor"""
        self._write(f"port {self.port} ; off\r")

    def deselect(self):
        """Unselect data from mode"""
        self._write(f"port {self.port} ; select\r")

    def _write(self, cmd):
        self.isconnected()
        Device._instance.write(cmd.encode())

    def _write1(self, data):
        hexstr = ' '.join(f'{h:x}' for h in data)
        self._write(f"port {self.port} ; write1 {hexstr}\r")

    def callback(self, func):
        """Set callback function

        :param func: Callback function
        """
        if func is not None:
            self.select()
        else:
            self.deselect()
        if func is None:
            self._conn.callit = None
        else:
            self._conn.callit = weakref.WeakMethod(func)

    @property
    def interval(self):
        """Interval between data points in milliseconds

        :getter: Gets interval
        :setter: Sets interval
        :return: Device interval
        :rtype: int
        """
        return self._interval

    @interval.setter
    def interval(self, value):
        """Interval between data points in milliseconds

        :param value: Interval
        :type value: int
        :raises DeviceError: Occurs if invalid interval passed
        """
        if isinstance(value, int) and value >= 0 and value <= 1000000000:
            self._interval = value
            self._write(f"port {self.port} ; selrate {self._interval}\r")
        else:
            raise DeviceError("Invalid interval")
