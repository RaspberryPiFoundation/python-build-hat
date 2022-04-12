from .serinterface import BuildHAT
from .exc import DeviceNotFound, DeviceChanged, DeviceInvalidMode, DeviceInvalid, PortInUse
import weakref
import time
import os
import sys
import gc

class Device:
    """Creates a single instance of the buildhat for all devices to use"""
    _instance = None
    _started = 0
    _device_names = {  1: ("PassiveMotor", "PassiveMotor"),
                       2: ("PassiveMotor", "PassiveMotor"),
                       8: ("Light", "Light"),                       # 88005
                      34: ("TiltSensor", "WeDo 2.0 Tilt Sensor"),   # 45305
                      35: ("MotionSensor", "MotionSensor"),         # 45304
                      37: ("ColorDistanceSensor", "Color & Distance Sensor"), # 88007
                      61: ("ColorSensor", "Color Sensor"),          # 45605
                      62: ("DistanceSensor","Distance Sensor"),     # 45604
                      63: ("ForceSensor", "Force Sensor"),          # 45606
                      64: ("Matrix", "3x3 Color Light Matrix"),     # 45608
                      38: ("Motor", "Medium Linear Motor"),         # 88008
                      46: ("Motor", "Large Motor"),                 # 88013
                      47: ("Motor", "XL Motor"),                    # 88014
                      48: ("Motor", "Medium Angular Motor (Cyan)"), # 45603
                      49: ("Motor", "Large Angular Motor (Cyan)"),  # 45602
                      65: ("Motor", "Small Angular Motor"),         # 45607
                      75: ("Motor", "Medium Angular Motor (Grey)"), # 88018
                      76: ("Motor", "Large Angular Motor (Grey)")   # 88017
                    }

    UNKNOWN_DEVICE = "Unknown"
    DISCONNECTED_DEVICE = "Disconnected"

    def __init__(self, port):
        if not isinstance(port, str) or len(port) != 1:
            raise DeviceNotFound("Invalid port")
        p = ord(port) - ord('A')
        if not (p >= 0 and p <= 3):
            raise DeviceNotFound("Invalid port")
        self.port = p
        Device._setup()
        if Device._instance is not None:
            if Device._instance._used[p] is not None:
	            raise PortInUse("Port already used")
        self._reset()
        Device._instance._used[p] = weakref.ref(self)

    def _reset(self):
        self._simplemode = -1
        self._combimode = -1
        self._typeid = self._conn.typeid
        if (self._typeid in Device._device_names and Device._device_names[self._typeid][0] != type(self).__name__) or self._typeid == -1:
            raise DeviceInvalid('There is not a {} connected to port {} (Found {})'.format(type(self).__name__, self.port, self.name))

    def _setup(device="/dev/serial0"):
        if Device._instance:
            return
        data = os.path.join(os.path.dirname(sys.modules["buildhat"].__file__),"data/")
        firm = os.path.join(data,"firmware.bin")
        sig = os.path.join(data,"signature.bin")
        ver = os.path.join(data,"version")
        vfile = open(ver)
        v = int(vfile.read())
        vfile.close()
        Device._instance = BuildHAT(firm, sig, v, device=device)
        weakref.finalize(Device._instance, Device._instance.shutdown)

    def __del__(self):
        if hasattr(self, "port") and (Device._instance._used[self.port] is not None):
            if Device._device_names[self._typeid][0] == "Matrix":
                self.clear()
            Device._instance._used[self.port] = None
            self._conn.callit = None
            self.deselect()
            if Device._device_names[self._typeid][0] != "Matrix":
                self.off()

    def name_for_id(typeid):
        """Translate integer type id to device name (python class)"""
        if typeid in Device._device_names:
            return Device._device_names[typeid][0]
        else:
            return Device.UNKNOWN_DEVICE

    def desc_for_id(typeid):
        """Translate integer type id to something more descriptive than the device name"""
        if typeid in Device._device_names:
            return Device._device_names[typeid][1]
        else:
            return Device.UNKNOWN_DEVICE

    @property
    def _conn(self):
        return Device._instance.connections[self.port]

    @property
    def connected(self):
        return self._conn.connected

    @property
    def typeid(self):
        return self._typeid

    @property
    def typeidcur(self):
        return self._conn.typeid

    @property
    def _hat(self):
        return Device._instance

    @property
    def name(self):
        """Determines name of device on port"""
        if self.connected == False:
            return Device.DISCONNECTED_DEVICE
        elif self.typeidcur in self._device_names:
            return self._device_names[self.typeidcur][0]
        else:
            return Device.UNKNOWN_DEVICE

    @property
    def description(self):
        """Description of device on port"""
        if self.connected == False:
            return Device.DISCONNECTED_DEVICE
        elif self.typeidcur in self._device_names:
            return self._device_names[self.typeidcur][1]
        else:
            return Device.UNKNOWN_DEVICE

    def isconnected(self):
        if not self.connected:
            raise DeviceNotFound("No device found")
        if self.typeid != self.typeidcur:
            raise DeviceChanged("Device has changed")

    def reverse(self):
        self._write("port {} ; plimit 1 ; set -1\r".format(self.port))

    def get(self):
        self.isconnected()
        idx = -1
        if self._simplemode != -1:
            idx = self._simplemode
        elif self._combimode != -1:
            idx = self._combimode
        else:
            raise DeviceInvalidMode("Not in simple or combimode")
        self._write("port {} ; selonce {}\r".format(self.port, idx))
        # wait for data
        with Device._instance.portcond[self.port]:
            Device._instance.portcond[self.port].wait()
        return self._conn.data

    def mode(self, modev):
        self.isconnected()
        if isinstance(modev, list):
            self._combimode = 0
            modestr = ""
            for t in modev:
                modestr += "{} {} ".format(t[0],t[1])
            self._write("port {} ; combi {} {}\r".format(self.port,self._combimode, modestr))
            self._simplemode = -1
        else:
            # Remove combi mode
            if self._combimode != -1:
                self._write("port {} ; combi {}\r".format(self.port,self._combimode))
            self._combimode = -1
            self._simplemode = int(modev)

    def select(self):
        self.isconnected()
        if self._simplemode != -1:
            idx = self._simplemode
        elif self._combimode != -1:
            idx = self._combimode
        else:
            raise DeviceInvalidMode("Not in simple or combimode")
        self._write("port {} ; select {}\r".format(self.port,idx))

    def on(self):
        """
        Turns on sensor
        """
        self._write("port {} ; plimit 1 ; on\r".format(self.port))

    def off(self):
        """
        Turns off sensor
        """
        self._write("port {} ; off\r".format(self.port))

    def deselect(self):
        self._write("port {} ; select\r".format(self.port))

    def _write(self, cmd):
        self.isconnected()
        Device._instance.write(cmd.encode())

    def _write1(self, data):
        self._write("port {} ; write1 {}\r".format(self.port, ' '.join('{:x}'.format(h) for h in data)))

    def callback(self, func):
        if func is not None:
            self.select()
        else:
            self.deselect()
        if func is None:
            self._conn.callit = None
        else:
            self._conn.callit = weakref.WeakMethod(func)
