import threading
import gpiozero
import serial
import time
from threading import Condition
from gpiozero import DigitalOutputDevice
from enum import Enum

CONNECTED = ": connected to active ID"
NOTCONNECTED = ": no device detected"
MOTOR_BIAS="bias .2"
MOTOR_PLIMIT="plimit .4"
PULSEDONE=": pulse done"
RAMPDONE=": ramp done"
FIRMWARE="Firmware version: "
BOOTLOADER="BuildHAT bootloader version"
PROMPT="BHBL>"

class HatNotFound(Exception):
    pass

class Device():

    def __init__(self, port, porto, buildhat):
        self.port = port
        self.porto = porto
        self.buildhat = buildhat
        self.callit = None
        self.FORMAT_SI = 0
        self.simplemode = -1
        self.combiindex = -1

    def reverse(self):
        self.buildhat.write("port {} ; plimit 1 ; set -1\r".format(self.port).encode())

    def get(self, ig):
        if self.simplemode != -1:
            self.buildhat.write("port {} ; selonce {}\r".format(self.port, self.simplemode).encode())
        else:
            self.buildhat.write("port {} ; selonce {}\r".format(self.port, self.combiindex).encode())
        # wait for data
        with self.buildhat.portcond[self.port]:
            self.buildhat.portcond[self.port].wait()
        return self.porto.data

    def mode(self, modev):
        if isinstance(modev, list):
            self.combiindex = 0
            modestr = ""
            for t in modev:
                modestr += "{} {} ".format(t[0],t[1])
            self.buildhat.write("port {} ; combi {} {}\r".format(self.port,self.combiindex, modestr).encode())
            self.simplemode = -1
        else:
            # Remove combi mode
            if self.combiindex != -1:
                self.buildhat.write("port {} ; combi {}\r".format(self.port,self.combiindex).encode())
            self.combiindex = -1
            self.simplemode = int(modev)
    
    def callback(self, func):
        if func is not None:

            if self.simplemode != -1:
                mode = "select {}".format(self.simplemode)
            elif self.combiindex != -1:
                mode = "select {}".format(self.combiindex)

            self.buildhat.write("port {} ; {}\r".format(self.port, mode).encode())
        self.callit = func

class InternalMotor:
    def __init__(self, port, porto, buildhat):
        self.porto = porto
        self.port = port
        self.buildhat = buildhat

    def get(self):
        self.buildhat.write("port {} ; selonce 0\r".format(self.port).encode())
        # wait for data
        with self.buildhat.portcond[self.port]:
            self.buildhat.portcond[self.port].wait()
        return self.porto.data

    def run_for_degrees(self, newpos, curpos, speed):
        #port 0 ; combi 0 1 0 2 0 3 0 ; select 0 ; plimit .5 ; bias .2 ; pid 0 0 1 s4 0.0027777778 0 5 0 .1 3 ; set ramp 1.000000 2.000000 0.033333 0

        cmd = "port {} ; combi 0 1 0 2 0 3 0 ; select 0 ; {} ; {} ; pid 0 0 1 s4 0.0027777778 0 5 0 .1 3 ; set ramp {} {} {} 0\r".format(self.port, MOTOR_PLIMIT, MOTOR_BIAS, curpos, 
                                                                                                                    newpos, (newpos - curpos) / speed).encode()
        self.buildhat.write(cmd);
        with self.buildhat.rampcond[self.port]:
            self.buildhat.rampcond[self.port].wait()

    def pwm(self, pwmv):
        self.buildhat.write("port {} ; pwm ; set {}\r".format(self.port,pwmv/100.0).encode())

    def float(self):
        self.pwm(0)

    def run_for_time(self, time, speed, blocking): 
        cmd = "port {} ; pwm ; {} ; {} ; set pulse {} 0.0 {} 0\r".format(self.port, MOTOR_PLIMIT, MOTOR_BIAS,speed/100.0, time/1000.0).encode();
        self.buildhat.write(cmd);
        if blocking:
            with self.buildhat.pulsecond[self.port]:
                self.buildhat.pulsecond[self.port].wait()

    def run_at_speed(self, speed):
        cmd = "port {} ; combi 0 1 0 2 0 3 0 ; select 0 ; {} ; {} ; pid 0 0 0 s1 1 0 0.003 0.01 0 100; set {}\r".format(self.port, MOTOR_PLIMIT, MOTOR_BIAS, speed).encode()
        self.buildhat.write(cmd)

class Port:
    def __init__(self, port, buildhat):
        self.data = []
        self.port = port
        self.buildhat = buildhat
        self.device = Device(port, self, self.buildhat)    
        self.motor = InternalMotor(port, self, self.buildhat)

    def info(self):    
        return {'type' : self.typeid}

class Ports:
    def __init__(self, buildhat):
        self.A = Port(0,buildhat)
        self.B = Port(1,buildhat)
        self.C = Port(2,buildhat)
        self.D = Port(3,buildhat)

class HatState(Enum):
    OTHER = 0
    FIRMWARE = 1
    NEEDNEWFIRMWARE = 2
    BOOTLOADER = 3

class BuildHAT:

    def __init__(self, firmware, signature, version):
        self.cond = Condition()
        self.state = HatState.OTHER
        self.portcond = []
        self.pulsecond = []
        self.rampcond = []

        for i in range(4):
            self.portcond.append(Condition())
            self.pulsecond.append(Condition())
            self.rampcond.append(Condition())

        self.ser = serial.Serial('/dev/serial0',115200, timeout=5)
        self.port = Ports(self)
        # Check if we're in the bootloader or the firmware
        self.write(b"version\r")
        while True:
            try:
                line = self.ser.readline().decode('utf-8')
                if len(line) == 0:
                    # Didn't recieve any data
                    break
                if line[:len(FIRMWARE)] == FIRMWARE:
                    self.state = HatState.FIRMWARE
                    ver =  line[len(FIRMWARE):].split(' ')
                    if int(ver[0]) == version:
                        self.state = HatState.FIRMWARE
                        break
                    else:
                        self.state = HatState.NEEDNEWFIRMWARE
                        break
                if line[:len(BOOTLOADER)] == BOOTLOADER:
                    self.state = HatState.BOOTLOADER
                    break
            except serial.SerialException:
                pass

        if self.state == HatState.NEEDNEWFIRMWARE:
            self.resethat()
            self.loadfirmware(firmware, signature)
        elif self.state == HatState.BOOTLOADER:
            self.loadfirmware(firmware, signature)
        elif self.state == HatState.OTHER:
            raise HatNotFound()

        th = threading.Thread(target=self.loop, args=(self.cond, ))
        th.daemon = True
        th.start()

        if self.state == HatState.FIRMWARE:
            self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")
            self.write(b"list\r")
        elif self.state == HatState.NEEDNEWFIRMWARE or self.state == HatState.BOOTLOADER:
            self.write(b"reboot\r")

        # wait for initialisation to finish
        with self.cond:
            self.cond.wait()

    def resethat(self):
        RESET_GPIO_NUMBER = 4
        BOOT0_GPIO_NUMBER = 22
        reset = DigitalOutputDevice(RESET_GPIO_NUMBER)
        boot0 = DigitalOutputDevice(BOOT0_GPIO_NUMBER)
        boot0.off()
        reset.off()
        time.sleep(0.01)
        reset.on()
        time.sleep(0.01)
        boot0.close()
        reset.close()
        time.sleep(0.5)

    def loadfirmware(self, firmware, signature):
        firm = open(firmware, "rb").read()
        sig = open(signature, "rb").read()
        self.write(b"clear\r")
        self.getprompt()
        self.write("load {} {}\r".format(len(firm), self.checksum(firm)).encode())
        time.sleep(0.1)
        self.write(b"\x02")
        self.write(firm)
        self.write(b"\x03\r")
        self.getprompt()
        self.write("signature {}\r".format(len(sig)).encode())
        time.sleep(0.1)
        self.write(b"\x02")
        self.write(sig)
        self.write(b"\x03\r")
        self.getprompt()

    def getprompt(self):
        # Need to decide what we will do, when no prompt
        while True:
            try:
                line = self.ser.readline().decode('utf-8')
                if line[:len(PROMPT)] == PROMPT:
                    break
            except serial.SerialException:
                pass

    def checksum(self, data):
        u = 1
        for i in range(0, len(data)):
            if (u & 0x80000000) != 0:
                u = (u << 1) ^ 0x1d872b41
            else:
                u = u << 1
            u = (u ^ data[i]) & 0xFFFFFFFF
        return u

    def write(self, data):
        self.ser.write(data)

    def loop(self, cond):
        count = 0
        while True:
            line = b""
            try:
                line = self.ser.readline().decode('utf-8')
            except serial.SerialException:
                pass
            if len(line) == 0:
                continue
            if line[0] == "P" and line[2] == ":":
                portid = int(line[1])
                port = getattr(self.port,chr(ord('A')+portid))
                if line[2:2+len(CONNECTED)] == CONNECTED:
                    typeid = int(line[2+len(CONNECTED):],16)
                    count += 1
                    port.typeid = typeid
                if line[2:2+len(NOTCONNECTED)] == NOTCONNECTED:
                    typeid = -1
                    count += 1
                    port.typeid = typeid
                if line[2:2+len(RAMPDONE)] == RAMPDONE:
                    with self.rampcond[portid]:
                        self.rampcond[portid].notify()
                if line[2:2+len(PULSEDONE)] == PULSEDONE:
                    with self.pulsecond[portid]:
                        self.pulsecond[portid].notify()
                if count == 4:
                    with cond:
                        cond.notify()
            if line[0] == "P" and (line[2] == "C" or line[2] == "M"):
                portid = int(line[1])
                port = getattr(self.port,chr(ord('A')+portid))
                data = line[5:].strip().split(" ")
                newdata = []
                for d in data:
                    if "." in d:
                        newdata.append(float(d))
                    else:
                        if d != "":
                            newdata.append(int(d))
                callit = port.device.callit
                if callit is not None:
                    callit(newdata)
                port.data = newdata
                with self.portcond[portid]:
                    self.portcond[portid].notify()