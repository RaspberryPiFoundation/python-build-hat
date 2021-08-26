from .serinterface import BuildHAT
import weakref
import time
import os
import sys
import gc

class Device:
    """Creates a single instance of the buildhat for all devices to use"""
    _instance = None
    _started = 0
    _device_names = { 61: "ColorSensor",
                      62: "DistanceSensor",
                      63: "ForceSensor",
                      64: "Matrix",
                      38: "Motor",
                      46: "Motor",
                      47: "Motor",
                      48: "Motor",
                      49: "Motor",
                      65: "Motor",
                      75: "Motor",
                      76: "Motor"
                    }
    
    def __init__(self, port):
        self.port = ord(port) - ord('A')
        if not Device._instance:
            data = os.path.join(os.path.dirname(sys.modules["buildhat"].__file__),"data/")
            firm = os.path.join(data,"firmware.bin")
            sig = os.path.join(data,"signature.bin")
            ver = os.path.join(data,"version")
            vfile = open(ver)
            v = int(vfile.read())
            vfile.close()
            Device._instance = BuildHAT(firm, sig, v)
            weakref.finalize(self, self.close)
        self.simplemode = -1
        self.combiindex = -1
        self._typeid = self._conn.typeid

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
        """Determine name of device on port"""
        if self.connected == False:
            return "No device"
        elif self.typeidcur in self._device_names:
            return self._device_names[self.typeidcur]
        else:
            return "Unknown"

    def close(self):
        Device._instance.shutdown()

    def isconnected(self):
        if not self.connected:
            raise DeviceNotFound("No device found")
        if self.typeid != self.typeidcur:
            raise DeviceChanged("Device has changed")

    def reverse(self):
        self.isconnected()
        Device._instance.write("port {} ; plimit 1 ; set -1\r".format(self.port).encode())

    def get(self):
        self.isconnected()
        if self.simplemode != -1:
            Device._instance.write("port {} ; selonce {}\r".format(self.port, self.simplemode).encode())
        else:
            Device._instance.write("port {} ; selonce {}\r".format(self.port, self.combiindex).encode())
        # wait for data
        with Device._instance.portcond[self.port]:
            Device._instance.portcond[self.port].wait()
        return self._conn.data

    def mode(self, modev):
        self.isconnected()
        if isinstance(modev, list):
            self.combiindex = 0
            modestr = ""
            for t in modev:
                modestr += "{} {} ".format(t[0],t[1])
            Device._instance.write("port {} ; combi {} {}\r".format(self.port,self.combiindex, modestr).encode())
            self.simplemode = -1
        else:
            # Remove combi mode
            if self.combiindex != -1:
                Device._instance.write("port {} ; combi {}\r".format(self.port,self.combiindex).encode())
            self.combiindex = -1
            self.simplemode = int(modev)

    def select(self):
        self.isconnected()
        if self.simplemode != -1:
            idx = self.simplemode
        if self.combiindex != -1:
            idx = self.combiindex
        if idx != -1:
            Device._instance.write("port {} ; select {}\r".format(self.port,idx).encode())

    def on(self):
        self.isconnected()
        Device._instance.write("port {} ; plimit 1 ; on\r".format(self.port).encode())

    def off(self):
        self.isconnected()
        Device._instance.write("port {} ; off\r".format(self.port).encode())

    def deselect(self):
        self.isconnected()
        Device._instance.write("port {} ; select\r".format(self.port).encode())

    def write(self, cmd):
        self.isconnected()
        Device._instance.write(cmd)

    def write1(self, data):
        self.isconnected()
        Device._instance.write("port {} ; write1 {}\r".format(self.port, ' '.join('{:x}'.format(h) for h in data)).encode())

    def callback(self, func):
        self.isconnected()
        if func is not None:
            if self.simplemode != -1:
                mode = "select {}".format(self.simplemode)
            elif self.combiindex != -1:
                mode = "select {}".format(self.combiindex)
            Device._instance.write("port {} ; {}\r".format(self.port, mode).encode())
        # should unselect if func is none I think
        self._conn.callit = func
