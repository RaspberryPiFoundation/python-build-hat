"""Build HAT handling functionality"""

import queue
import threading
import time
import weakref
from enum import Enum
from threading import Condition, Lock, Timer

import serial
from gpiozero import DigitalOutputDevice

from .exc import BuildHATError, DeviceError


class HatState(Enum):
    """Current state that hat is in"""

    OTHER = 0
    FIRMWARE = 1
    NEEDNEWFIRMWARE = 2
    BOOTLOADER = 3


class CommandState(Enum):
    """Tracking state for sent serial commands"""

    SENT = 0
    FINISHED = 1
    CONFIRMED = 2
    NONE = 3


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
    DEVCONNECTING = ": connecting to active device"                       # Beginning of list
    DEVSERIALCOMM = ": established serial communication with active ID"   # end of list
    DEVCHECKSUMERR = ": checksum error: disconnecting"
    DEVBAUDPREAMBLE = "set baud rate to 115200"

    DEVICE_STARTUP_DELAY = 2

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
        self.listcond = Condition()
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
        self.time_when_started = time.time()
        self.time_when_ready = None

        # Class instances using each port
        self._use_lock = Lock()
        self._used = [None, None, None, None]

        for _ in range(4):
            self.connections.append(Connection())
            self.portcond.append(Condition())
            self.pulsecond.append(Condition())
            self.rampcond.append(Condition())

        self.ser = serial.Serial(device, 115200, timeout=5)

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
            if self.state == HatState.OTHER or self.state == HatState.NEEDNEWFIRMWARE:
                raise BuildHATError("HAT not found")

        self.cbqueue = queue.Queue()
        self.cb = threading.Thread(target=self.callbackloop, args=(self.cbqueue,))
        self.cb.daemon = True
        self.cb.start()

        # Drop timeout value to 1s
        self.ser.timeout = 1
        self.th = threading.Thread(target=self.loop, args=(self.listcond, self.state == HatState.FIRMWARE, self.cbqueue))
        self.th.daemon = True
        self.th.start()

        if self.state == HatState.NEEDNEWFIRMWARE or self.state == HatState.BOOTLOADER:
            self.write(b"reboot\r")

        # wait for initialisation to finish
        with self.listcond:
            self.listcond.wait()

        self.time_when_ready = time.time()

    def _what_state_are_we_in(self):
        """Is the BuildHAT in the bootloader or the firmware?"""
        self.ser.timeout = 1  # Run the timeout fast because junk gets tossed
        wait_for_version_timeout = 5
        detected_state = HatState.OTHER
        version_status = CommandState.NONE
        version_retries = 0
        junklines = 0
        device_chatter = False
        while True:
            if version_status == CommandState.NONE:
                # Can't wait until the serial port calms down (len(line) == 0)
                # because BuildHAT could be spewing select data
                self.write(b"version\r")
                version_time = time.time()
                version_status = CommandState.SENT

            try:
                line = self.ser.readline().decode('utf-8', 'ignore')
            except serial.SerialException as e:
                # Escalate, don't ignore
                raise e

            if line == "\r\n" and version_status == CommandState.SENT:
                # When initially connecting to the serial port, a CRLF is returned
                # after "version" is sent and nothing happens.  Sending another
                # CR gets the command to go through
                # This doesn't happen if you create an entirely new BuildHAT object, though,
                # The extra CR is registered as Error
                self.write(b"\r")
                version_status = CommandState.CONFIRMED
                continue

            if cmp(line, "Error") and version_status == CommandState.CONFIRMED:
                # BuildHAT was ready and it was just sent it a \r like it was not ready
                # So, start over!
                version_status = CommandState.NONE
                continue

            if line == "version\r\n" and version_status == CommandState.SENT:
                # When initially connecting to the serial port in the
                # bootloader, it will just echo your command with a CRLF
                # and do nothing, so just send it again
                version_status = CommandState.NONE
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
                if len(line) != 0:
                    # got other data we didn't understand
                    if not (line == "\r\n"):
                        # Ignore unidentified junk except...
                        if line[0] == "P" and line[2] == ":" and cmp(line[2:], BuildHAT.DEVCONNECTING):
                            # A device is connecting during detection.
                            device_chatter = True
                        elif line[0] == " ":
                            # Probably a capability listing
                            device_chatter = True
                        elif cmp(line, BuildHAT.DEVBAUDPREAMBLE):
                            # Definitely a device connecting
                            device_chatter = True
                        junklines += 1
                        pass
                    # else ignore newlines
                # else ignore no-data

            if version_status != CommandState.NONE and time.time() - version_time > wait_for_version_timeout:
                version_status = CommandState.NONE
                version_retries += 1
            if version_retries > 5:
                # Give up after (version_retries * wait_for_version_timeout) sec and leave in HatState.OTHER
                break

        if device_chatter:
            # Sleep for a bit to wait for whatever was chatting on the serial line to settle
            time.sleep(BuildHAT.DEVICE_STARTUP_DELAY)

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
        time.sleep(0.1)
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

    def use_device(self, dev):
        """Register a class as using a port"""

        self._use_lock.acquire()
        try:
            if self._used[dev.port] is None:
                self._used[dev.port] = weakref.ref(dev)
            else:
                raise DeviceError("Attempted to instantiate {} on port {} where a {} was already connected".format(
                    dev.name(), dev.port, self._used[dev.port]().name()))
        finally:
            self._use_lock.release()

    def disuse_device(self, port):
        """Unregister a class as using a port"""

        # Stop tracking device class because either:
        # A: The class has been deleted or
        # B: Signal from serial port that the backing device is gone
        self._use_lock.acquire()
        try:
            if self._used[port] is not None:
                self._used[port]()._shutdown()
            self._used[port] = None
        finally:
            self._use_lock.release()

    def is_port_in_use(self, port):
        """Indicate if a port is in use by a class"""

        return self._used[port] is not None

    def reset(self):
        """Reset the BuildHAT"""
        # Stop the main loop
        self.running = False
        self.th.join()

        # Trigger running evaluation
        self.cbqueue.put(())
        self.cb.join()

        # Safe-shutdown, but don't delete
        self._use_lock.acquire()
        try:
            for d in range(4):
                if self._used[d] is not None:
                    self._used[d]()._shutdown()
        finally:
            self._use_lock.release()

        self.time_when_ready = None

        self.ser.timeout = 1
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

        # Device instances will get signaled they've been reset (_startup()) in the loop

        # Start the loop serial input processor
        self.ser.timeout = 1
        self.th = threading.Thread(target=self.loop, args=(self.listcond, False, self.cbqueue))
        self.th.daemon = True
        self.th.start()

        # Wait for the serial input thread signal that the ports are enumerated
        with self.listcond:
            self.listcond.wait()

        self.time_when_ready = time.time()

    def shutdown(self):
        """Turn off the Build HAT devices"""
        if not self.fin:
            self.fin = True
            self.running = False
            self.th.join()
            self.cbqueue.put(())
            self.cb.join()
            self._use_lock.acquire()
            try:
                for d in range(4):
                    if self._used[d] is not None:
                        self._used[d]()._shutdown()
                    self._used[d] = None
            finally:
                self._use_lock.release()
            self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")
            self.ser.close()

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

    def loop(self, listcond, uselist, q):
        """Event handling for Build HAT

        :param cond: Condition used to block user's script till we're ready
        :param uselist: Whether we're using the HATs 'list' function or not
        :param q: Queue for callback events
        """
        listing_status = CommandState.NONE

        if uselist:
            # BuildHAT is in firmware and ports need to be enumerated from a listing
            listing_status = CommandState.SENT
            self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")
            self.write(b"list\r")
            # Send version so loop knows when the list is complete
            self.write(b"version\r")
        # else didn't have firmware (and was rebooted): use the auto-issued port listing after BuildHAT.DONE

        count = 0
        device_is_connecting = 0
        stalled_device_timeout = 6
        stoploop = False
        while not stoploop:
            line = b""
            try:
                line = self.ser.readline().decode('utf-8', 'ignore')
            except serial.SerialException as e:
                raise e

            if len(line) > 0 and line[0] == "P" and line[2] == ":":
                # Port status changes
                portid = int(line[1])
                device = None
                if self._used[portid] is not None:
                    device = self._used[portid]()
                msg = line[2:]
                if cmp(msg, BuildHAT.CONNECTED):
                    # Listing header OR
                    # On bootup or dynamic detection, comes after baud preamble
                    # but before before serial comm confirmation
                    typeid = int(line[2 + len(BuildHAT.CONNECTED):], 16)
                    self.connections[portid].update(typeid, True)
                    if device:
                        device._startup()
                    else:
                        # Matrix needs to be turned on as soon as it's detected
                        # since it can't wait for the class to be lazily instantiated
                        # according to legend
                        if typeid == 64:
                            self.write("plimit 1; port {} ; on\r".format(portid).encode())
                    if uselist and listing_status == CommandState.SENT:
                        count += 1
                    device_is_connecting = 0
                elif cmp(msg, BuildHAT.CONNECTEDPASSIVE):
                    # Only confirmation that the passive device was detected
                    typeid = int(line[2 + len(BuildHAT.CONNECTEDPASSIVE):], 16)
                    self.connections[portid].update(typeid, True)
                    if device:
                        device._startup()
                    if uselist and listing_status == CommandState.SENT:
                        count += 1
                elif cmp(msg, BuildHAT.DEVCONNECTING):
                    # Device detected and connecting (dynamic or bootup)
                    # Just reset the time clock for waiting on it
                    device_is_connecting = time.time()
                elif cmp(msg, BuildHAT.DEVSERIALCOMM):
                    # Confirmation of a new connection (dynamic or on bootup, not on listing)
                    typeid = int(line[2 + len(BuildHAT.DEVSERIALCOMM):], 16)
                    device_is_connecting = 0
                elif cmp(msg, BuildHAT.DISCONNECTED):
                    self.connections[portid].update(-1, False)
                    device_is_connecting = 0
                elif cmp(msg, BuildHAT.DEVCHECKSUMERR):
                    self.connections[portid].update(-1, False)
                    device_is_connecting = 0
                elif cmp(msg, BuildHAT.DEVTIMEOUT):
                    self.connections[portid].update(-1, False)
                    device_is_connecting = 0
                elif cmp(msg, BuildHAT.NOTCONNECTED):
                    self.connections[portid].update(-1, False)
                    if uselist and listing_status == CommandState.SENT:
                        count += 1
                elif cmp(msg, BuildHAT.RAMPDONE):
                    with self.rampcond[portid]:
                        self.rampcond[portid].notify()
                elif cmp(msg, BuildHAT.PULSEDONE):
                    with self.pulsecond[portid]:
                        self.pulsecond[portid].notify()

            if len(line) > 0 and cmp(line, BuildHAT.DEVBAUDPREAMBLE):
                # Device is establishing serial connection after DEVCONNECTING
                # Some device is definitely coming online...
                device_is_connecting = time.time()

            if len(line) >= 5 and line[1] == "." and line.strip().endswith(" V"):
                vin = float(line.strip().split(" ")[0])
                self.vin = vin
                with self.vincond:
                    self.vincond.notify()

            if len(line) > 0 and line[0] == "P" and (line[2] == "C" or line[2] == "M"):
                # Specific port mode information
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

            if listing_status == CommandState.SENT:
                if cmp(line, BuildHAT.FIRMWARE):
                    # Found version line, indicates that the list command finished
                    listing_status = CommandState.FINISHED

            if listing_status == CommandState.FINISHED:
                if uselist:
                    if count >= 4:
                        # > 4 means a device could have come online while listing
                        listing_status = CommandState.NONE
                        with listcond:
                            uselist = False
                            listcond.notify()
                else:
                    # Don't know how you got here
                    raise BuildHATError("Finished a listing after a reboot that did not issue a listing")

            if not uselist and cmp(line, BuildHAT.DONE):
                def runit():
                    with listcond:
                        listcond.notify()
                # On reboot, wait for the port listing to complete before notifying
                # Can't be counted out because the reboot listing doesn't list disconnected ports
                t = Timer(8.0, runit)
                t.start()

            if device_is_connecting != 0 and (time.time() - device_is_connecting > stalled_device_timeout):
                # Don't let stalled device detection block startup otherwise get stuck in a loop
                device_is_connecting = 0

            if self.running is False:
                # Quit loop if asked
                if device_is_connecting == 0:
                    # ... unless the loop is waiting for a device to connect
                    if listing_status == CommandState.NONE:
                        # ... or waiting for a listing to finish
                        if time.time() - self.time_when_started > BuildHAT.DEVICE_STARTUP_DELAY:
                            # ... OR the BuildHAT was destroyed too soon after starting it
                            stoploop = True
                        # else not stopping until the startup delay has passed
                    # else not stopping until the device listing is finished
                # else not stopping if a device is in the process of connecting
