"""Build HAT handling functionality"""

import queue
import threading
import time
from enum import Enum
from threading import Condition, Timer

import serial
from gpiozero import DigitalOutputDevice

from .exc import BuildHATError


class HatState(Enum):
    """Current state that hat is in"""

    OTHER = 0
    FIRMWARE = 1
    NEEDNEWFIRMWARE = 2
    BOOTLOADER = 3


class Connection:
    """Connection information for a port"""

    def __init__(self):
        """Initialise connection"""
        self.typeid = -1
        self.connected = False
        self.callit = None
        self.data = None

    def update(self, typeid, connected, callit=None):
        """Update connection information for port

        :param typeid: Type ID of device on port
        :param connected: Whether device is connected or not
        :param callit: Callback function
        """
        self.typeid = typeid
        self.connected = connected
        self.callit = callit


def cmp(str1, str2):
    """Look for str2 in str1

    :param str1: String to look in
    :param str2: String to look for
    :return: Whether str2 exists
    """
    return str1[:len(str2)] == str2


class BuildHAT:
    """Interacts with Build HAT via UART interface"""

    CONNECTED = ": connected to active ID"
    CONNECTEDPASSIVE = ": connected to passive ID"
    DISCONNECTED = ": disconnected"
    DEVTIMEOUT = ": timeout during data phase: disconnecting"
    NOTCONNECTED = ": no device detected"
    PULSEDONE = ": pulse done"
    RAMPDONE = ": ramp done"
    FIRMWARE = "Firmware version: "
    BOOTLOADER = "BuildHAT bootloader version"
    DONE = "Done initialising ports"
    PROMPT = "BHBL>"
    RESET_GPIO_NUMBER = 4
    BOOT0_GPIO_NUMBER = 22

    _firmware_file = None
    _firmware_signature = None
    _firmware_version = None

    def __init__(self, firmware, signature, version, device="/dev/serial0"):
        """Interact with Build HAT

        :param firmware: Firmware file
        :param signature: Signature file
        :param version: Firmware version
        :param device: Serial device to use
        :raises BuildHATError: Occurs if can't find HAT
        """
        self.cond = Condition()
        self.state = HatState.OTHER
        self.connections = []
        self.portcond = []
        self.pulsecond = []
        self.rampcond = []
        self.fin = False
        self.running = True
        self.vincond = Condition()
        self.vin = None
        self._firmware_file = firmware
        self._firmware_signature = signature
        self._firmware_version = version
        # Class instances using each port
        self._used = [None, None, None, None]

        for _ in range(4):
            self.connections.append(Connection())
            self.portcond.append(Condition())
            self.pulsecond.append(Condition())
            self.rampcond.append(Condition())

        self.ser = serial.Serial(device, 115200, timeout=5)
        # Check if we're in the bootloader or the firmware
        self.state = self._what_state_are_we_in()

        if self.state == HatState.NEEDNEWFIRMWARE:
            self.resethat()
            self.loadfirmware()
        elif self.state == HatState.BOOTLOADER:
            self.loadfirmware()
        elif self.state == HatState.OTHER:
            # HAT not responding?  Try GPIO rebooting it before giving up
            self.resethat()
            self.loadfirmware()
            self.state = self._what_state_are_we_in()
            if self.state == HatState.OTHER:
                raise BuildHATError("HAT not found")

        self.cbqueue = queue.Queue()
        self.cb = threading.Thread(target=self.callbackloop, args=(self.cbqueue,))
        self.cb.daemon = True
        self.cb.start()

        # Drop timeout value to 1s
        self.ser.timeout = 1
        self.th = threading.Thread(target=self.loop, args=(self.cond, self.state == HatState.FIRMWARE, self.cbqueue))
        self.th.daemon = True
        self.th.start()

        if self.state == HatState.NEEDNEWFIRMWARE or self.state == HatState.BOOTLOADER:
            self.write(b"reboot\r")

        # wait for initialisation to finish
        with self.cond:
            self.cond.wait()

    def _what_state_are_we_in(self):
        """Is the BuildHAT in the bootloader or the firmware?"""
        self.ser.timeout = 5
        detected_state = HatState.OTHER
        self.write(b"version\r")
        incdata = 0
        while True:
            try:
                line = self.ser.readline().decode('utf-8', 'ignore')
            except serial.SerialException:
                pass
            if len(line) == 0:
                break
            if line == "\r\n":
                # When initially connecting to the serial port a CRLF is
                # returned when the version is sent, so just send it again
                self.write(b"version\r")
                continue
            if line == "version\r\n":
                # When initially connecting to the serial port in the
                # bootloader, it will just echo your command with a CRLF
                # and do nothing, so just send it again
                self.write(b"version\r")
                continue

            if line[:len(BuildHAT.FIRMWARE)] == BuildHAT.FIRMWARE:
                ver = line[len(BuildHAT.FIRMWARE):].split(' ')
                if int(ver[0]) == self._firmware_version:
                    detected_state = HatState.FIRMWARE
                else:
                    detected_state = HatState.NEEDNEWFIRMWARE
                break
            elif line[:len(BuildHAT.BOOTLOADER)] == BuildHAT.BOOTLOADER:
                # HAT does not have any firmware loaded or is stuck in the bootloader
                detected_state = HatState.BOOTLOADER
                break

            else:
                # got other data we didn't understand - send version again
                incdata += 1
                if incdata > 5:
                    break
                else:
                    self.write(b"version\r")
        self.ser.timeout = 1
        return detected_state

    def resethat(self):
        """Reset the HAT"""
        reset = DigitalOutputDevice(BuildHAT.RESET_GPIO_NUMBER)
        boot0 = DigitalOutputDevice(BuildHAT.BOOT0_GPIO_NUMBER)
        boot0.off()
        reset.off()
        time.sleep(0.01)
        reset.on()
        time.sleep(0.01)
        boot0.close()
        reset.close()
        time.sleep(0.5)

    def loadfirmware(self):
        """Load firmware

        :param firmware: Firmware to load
        :param signature: Signature to load
        """
        with open(self._firmware_file, "rb") as f:
            firm = f.read()
        with open(self._firmware_signature, "rb") as f:
            sig = f.read()
        self.write(b"clear\r")
        self.getprompt()
        self.write("load {} {}\r".format(len(firm), self.checksum(firm)).encode())
        time.sleep(0.1)
        self.write(b"\x02")
        self.write(firm)
        self.write(b"\x03\r")
        time.sleep(0.1)
        self.getprompt()
        self.write("signature {}\r".format(len(sig)).encode())
        time.sleep(0.1)
        self.write(b"\x02")
        self.write(sig)
        self.write(b"\x03\r")
        time.sleep(0.1)
        self.getprompt()

    def getprompt(self):
        """Loop until prompt is found

        Need to decide what we will do, when no prompt
        """
        while True:
            line = b""
            try:
                line = self.ser.readline().decode('utf-8', 'ignore')
            except serial.SerialException:
                pass
            if line[:len(BuildHAT.PROMPT)] == BuildHAT.PROMPT:
                break

    def checksum(self, data):
        """Calculate checksum from data

        :param data: Data to calculate the checksum from
        :return: Checksum that has been calculated
        """
        u = 1
        for i in range(0, len(data)):
            if (u & 0x80000000) != 0:
                u = (u << 1) ^ 0x1d872b41
            else:
                u = u << 1
            u = (u ^ data[i]) & 0xFFFFFFFF
        return u

    def write(self, data):
        """Write data to the serial port of Build HAT

        :param data: Data to write to Build HAT
        """
        self.ser.write(data)

    def reboot(self):
        """Reboot the BuildHAT"""
        # Stop the main loop
        self.running = False
        self.th.join()

        # Trigger running evaluation
        self.cbqueue.put(())
        self.cb.join()
        self.ser.timeout = 5
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()
        self.resethat()
        self.loadfirmware()

        # Reboot out of the bootloader into the firmware
        self.write(b"reboot\r")
        self.running = True

        # Restart callback queue
        self.cbqueue = queue.Queue()
        self.cb = threading.Thread(target=self.callbackloop, args=(self.cbqueue,))
        self.cb.daemon = True
        self.cb.start()

        # Start the loop serial input processor
        self.ser.timeout = 1
        self.th = threading.Thread(target=self.loop, args=(self.cond, False, self.cbqueue))
        self.th.daemon = True
        self.th.start()

        # Wait for the serial input thread signal that the ports are enumerated
        with self.cond:
            self.cond.wait()

        # Signal all device instances that they've been reset
        for devref in self._used:
            if devref is not None:
                device = devref()  # pylint: disable=not-callable
                device._reset()

    def shutdown(self):
        """Turn off the Build HAT devices"""
        if not self.fin:
            self.fin = True
            self.running = False
            self.th.join()
            self.cbqueue.put(())
            self.cb.join()
            turnoff = ""
            for p in range(4):
                conn = self.connections[p]
                if conn.typeid != 64:
                    turnoff += "port {} ; pwm ; coast ; off ;".format(p)
                else:
                    self.write("port {} ; write1 {}\r".format(p, ' '.join('{:x}'.format(h)
                                                              for h in [0xc2, 0, 0, 0, 0, 0, 0, 0, 0, 0])).encode())
            self.write("{}\r".format(turnoff).encode())
            self._used = [None, None, None, None]
            self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")

    def callbackloop(self, q):
        """Event handling for callbacks

        :param q: Queue of callback events
        """
        while self.running:
            cb = q.get()
            # Test for empty tuple, which should only be passed when
            # we're shutting down
            if len(cb) == 0:
                continue
            if not cb[0]._alive:
                continue
            cb[0]()(cb[1])
            q.task_done()

    def loop(self, cond, uselist, q):
        """Event handling for Build HAT

        :param cond: Condition used to block user's script till we're ready
        :param uselist: Whether we're using the HATs 'list' function or not
        :param q: Queue for callback events
        """
        if uselist:
            # Ports need to be enumerated from a listing
            self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")
            self.write(b"list\r")
        # else didn't have firmware (rebooted) so use the port listing that's auto-issued after BuildHAT.DONE

        count = 0
        while self.running:
            line = b""
            try:
                line = self.ser.readline().decode('utf-8', 'ignore')
            except serial.SerialException:
                pass
            if len(line) == 0:
                continue
            if line[0] == "P" and line[2] == ":":
                portid = int(line[1])
                msg = line[2:]
                if cmp(msg, BuildHAT.CONNECTED):
                    typeid = int(line[2 + len(BuildHAT.CONNECTED):], 16)
                    self.connections[portid].update(typeid, True)
                    if typeid == 64:
                        self.write("port {} ; on\r".format(portid).encode())
                    if uselist:
                        count += 1
                elif cmp(msg, BuildHAT.CONNECTEDPASSIVE):
                    typeid = int(line[2 + len(BuildHAT.CONNECTEDPASSIVE):], 16)
                    self.connections[portid].update(typeid, True)
                    if uselist:
                        count += 1
                elif cmp(msg, BuildHAT.DISCONNECTED):
                    self.connections[portid].update(-1, False)
                elif cmp(msg, BuildHAT.DEVTIMEOUT):
                    self.connections[portid].update(-1, False)
                elif cmp(msg, BuildHAT.NOTCONNECTED):
                    self.connections[portid].update(-1, False)
                    if uselist:
                        count += 1
                elif cmp(msg, BuildHAT.RAMPDONE):
                    with self.rampcond[portid]:
                        self.rampcond[portid].notify()
                elif cmp(msg, BuildHAT.PULSEDONE):
                    with self.pulsecond[portid]:
                        self.pulsecond[portid].notify()

            if uselist and count == 4:
                with cond:
                    uselist = False
                    cond.notify()

            if not uselist and cmp(line, BuildHAT.DONE):
                def runit():
                    with cond:
                        cond.notify()
                # On reboot, wait for the port listing to complete before notifying
                # Can't be counted out because the reboot listing doesn't list disconnected ports
                t = Timer(8.0, runit)
                t.start()

            if line[0] == "P" and (line[2] == "C" or line[2] == "M"):
                portid = int(line[1])
                data = line[5:].strip().split(" ")
                newdata = []
                for d in data:
                    if "." in d:
                        newdata.append(float(d))
                    else:
                        if d != "":
                            newdata.append(int(d))
                callit = self.connections[portid].callit
                if callit is not None:
                    q.put((callit, newdata))
                self.connections[portid].data = newdata
                with self.portcond[portid]:
                    self.portcond[portid].notify()

            if len(line) >= 5 and line[1] == "." and line.strip().endswith(" V"):
                vin = float(line.strip().split(" ")[0])
                self.vin = vin
                with self.vincond:
                    self.vincond.notify()
