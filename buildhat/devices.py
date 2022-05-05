"""Functionality for handling Build HAT devices"""

import os
import sys
import weakref

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
        self.port = p
        Device._setup()
        if Device._instance.is_port_in_use(self.port):
            raise DeviceError("Port already used")
        self._startup()
        Device._instance.use_device(self)

    def _startup(self):
        self._simplemode = -1
        self._combimode = -1
        self._typeid = self._conn.typeid
        if (
            self._typeid in Device._device_names
            and Device._device_names[self._typeid][0] != type(self).__name__  # noqa: W503
        ) or self._typeid == -1:
            raise DeviceError('There is not a {} connected to port {} (Found {})'.format(type(self).__name__,
                                                                                         self.port, self.name))

    @staticmethod
    def _setup(**kwargs):
        if Device._instance:
            return
        data = os.path.join(os.path.dirname(sys.modules["buildhat"].__file__), "data/")
        firm = os.path.join(data, "firmware.bin")
        sig = os.path.join(data, "signature.bin")
        ver = os.path.join(data, "version")
        vfile = open(ver)
        v = int(vfile.read())
        vfile.close()
        Device._instance = BuildHAT(firm, sig, v, **kwargs)
        weakref.finalize(Device._instance, Device._instance.shutdown)

    def _shutdown(self):
        if self.connected:
            # Send commands for clean shutdown
            # BuildHAT calls this when managing port in-use list
            if Device._instance.is_port_in_use(self.port):
                self.deselect()
                self.off()
            else:
                raise DeviceError("Attempted to shutdown a device that was not in use")

    def __del__(self):
        """Handle deletion of device"""
        # Delete yourself from the in-use list
        if Device._instance:
            if Device._instance.is_port_in_use(self.port):
                # Get deleted from the in-use list from the BuildHAT class
                Device._instance.disuse_device(self.port)
            self._conn.callit = None

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
        self._write("port {} ; plimit 1 ; set -1\r".format(self.port))

    def get(self):
        """Extract information from device

        :return: Data from device
        :raises DeviceError: Occurs if device not in valid mode
        """
        self.isconnected()
        idx = -1
        if self._simplemode != -1:
            idx = self._simplemode
        elif self._combimode != -1:
            idx = self._combimode
        else:
            raise DeviceError("Not in simple or combimode")
        self._write("port {} ; selonce {}\r".format(self.port, idx))
        # wait for data
        with Device._instance.portcond[self.port]:
            Device._instance.portcond[self.port].wait()
        return self._conn.data

    def mode(self, modev):
        """Set combimode or simple mode

        :param modev: List of tuples for a combimode, or integer for simple mode
        """
        self.isconnected()
        if isinstance(modev, list):
            self._combimode = 0
            modestr = ""
            for t in modev:
                modestr += "{} {} ".format(t[0], t[1])
            self._write("port {} ; combi {} {}\r".format(self.port, self._combimode, modestr))
            self._simplemode = -1
        else:
            # Remove combi mode
            if self._combimode != -1:
                self._write("port {} ; combi {}\r".format(self.port, self._combimode))
            self._combimode = -1
            self._simplemode = int(modev)

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
        self._write("port {} ; select {}\r".format(self.port, idx))

    def on(self):
        """Turn on sensor"""
        self._write("port {} ; plimit 1 ; on\r".format(self.port))

    def off(self):
        """Turn off sensor"""
        self._write("port {} ; off\r".format(self.port))

    def deselect(self):
        """Unselect data from mode"""
        self._write("port {} ; select\r".format(self.port))

    def _write(self, cmd):
        self.isconnected()
        Device._instance.write(cmd.encode())

    def _write1(self, data):
        self._write("port {} ; write1 {}\r".format(self.port, ' '.join('{:x}'.format(h) for h in data)))

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
