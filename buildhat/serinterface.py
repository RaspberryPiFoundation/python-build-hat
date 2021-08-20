import threading
import gpiozero
import serial
import time
import queue
from threading import Condition, Timer
from gpiozero import DigitalOutputDevice
from enum import Enum

class HatNotFound(Exception):
    pass

class Device:
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
    MOTOR_BIAS="bias .2"
    MOTOR_PLIMIT="plimit .4"

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
        cmd = "port {} ; combi 0 1 0 2 0 3 0 ; select 0 ; {} ; {} ; pid 0 0 1 s4 0.0027777778 0 5 0 .1 3 ; set ramp {} {} {} 0\r".format(self.port, InternalMotor.MOTOR_PLIMIT, InternalMotor.MOTOR_BIAS, curpos,
                                                                                                                                  newpos, (newpos - curpos) / speed).encode()
        self.buildhat.write(cmd);
        with self.buildhat.rampcond[self.port]:
            self.buildhat.rampcond[self.port].wait()

    def pwm(self, pwmv):
        self.buildhat.write("port {} ; pwm ; set {}\r".format(self.port,pwmv/100.0).encode())

    def coast(self):
        self.buildhat.write("port {} ; coast\r".format(self.port).encode())

    def float(self):
        self.pwm(0)

    def run_for_time(self, time, speed, blocking): 
        cmd = "port {} ; pwm ; {} ; {} ; set pulse {} 0.0 {} 0\r".format(self.port, InternalMotor.MOTOR_PLIMIT, InternalMotor.MOTOR_BIAS,speed/100.0, time/1000.0).encode();
        self.buildhat.write(cmd);
        if blocking:
            with self.buildhat.pulsecond[self.port]:
                self.buildhat.pulsecond[self.port].wait()

    def run_at_speed(self, speed):
        cmd = "port {} ; combi 0 1 0 2 0 3 0 ; select 0 ; {} ; {} ; pid 0 0 0 s1 1 0 0.003 0.01 0 100; set {}\r".format(self.port, InternalMotor.MOTOR_PLIMIT, InternalMotor.MOTOR_BIAS, speed).encode()
        self.buildhat.write(cmd)

class Port:
    def __init__(self, port, buildhat):
        self.data = []
        self.port = port
        self.buildhat = buildhat
        self.typeid = -1
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
    CONNECTED=": connected to active ID"
    DISCONNECTED=": disconnected"
    NOTCONNECTED=": no device detected"
    PULSEDONE=": pulse done"
    RAMPDONE=": ramp done"
    FIRMWARE="Firmware version: "
    BOOTLOADER="BuildHAT bootloader version"
    DONE="Done initialising ports"

    def __init__(self, firmware, signature, version):
        self.cond = Condition()
        self.state = HatState.OTHER
        self.portcond = []
        self.pulsecond = []
        self.rampcond = []
        self.fin = False
        self.running = True

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
                if line[:len(BuildHAT.FIRMWARE)] == BuildHAT.FIRMWARE:
                    self.state = HatState.FIRMWARE
                    ver =  line[len(BuildHAT.FIRMWARE):].split(' ')
                    if int(ver[0]) == version:
                        self.state = HatState.FIRMWARE
                        break
                    else:
                        self.state = HatState.NEEDNEWFIRMWARE
                        break
                if line[:len(BuildHAT.BOOTLOADER)] == BuildHAT.BOOTLOADER:
                    self.state = HatState.BOOTLOADER
                    break
            except serial.SerialException:
                pass

        # Use to force hat reset
        #self.state = HatState.NEEDNEWFIRMWARE
        if self.state == HatState.NEEDNEWFIRMWARE:
            self.resethat()
            self.loadfirmware(firmware, signature)
        elif self.state == HatState.BOOTLOADER:
            self.loadfirmware(firmware, signature)
        elif self.state == HatState.OTHER:
            raise HatNotFound()

        self.cbqueue = queue.Queue()
        self.cb = threading.Thread(target=self.callbackloop, args=(self.cbqueue,))
        self.cb.daemon = True
        self.cb.start()

        # Drop timeout value to 1s
        self.ser.timeout = 1
        self.th = threading.Thread(target=self.loop, args=(self.cond, self.state == HatState.FIRMWARE, self.cbqueue))
        self.th.daemon = True
        self.th.start()

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
        PROMPT="BHBL>"
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

    def shutdown(self):
        if not self.fin:
            self.fin = True
            self.running = False
            self.th.join()
            self.write(b"port 0 ; pwm ; off ; port 1 ; pwm ; off ; port 2 ; pwm ; off ; port 3 ; pwm ; off\r")
            #self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")

    def callbackloop(self, q):
        while self.running:
            cb = q.get()
            cb[0](cb[1])
            q.task_done()

    def loop(self, cond, uselist, q):
        count = 0
        while self.running:
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
                if line[2:2+len(BuildHAT.CONNECTED)] == BuildHAT.CONNECTED:
                    typeid = int(line[2+len(BuildHAT.CONNECTED):],16)
                    port.typeid = typeid
                    if uselist:
                        count += 1
                if line[2:2+len(BuildHAT.DISCONNECTED)] == BuildHAT.DISCONNECTED:
                    typeid = -1
                    port.typeid = typeid
                if line[2:2+len(BuildHAT.NOTCONNECTED)] == BuildHAT.NOTCONNECTED:
                    typeid = -1
                    port.typeid = typeid
                    if uselist:
                        count += 1
                if line[2:2+len(BuildHAT.RAMPDONE)] == BuildHAT.RAMPDONE:
                    with self.rampcond[portid]:
                        self.rampcond[portid].notify()
                if line[2:2+len(BuildHAT.PULSEDONE)] == BuildHAT.PULSEDONE:
                    with self.pulsecond[portid]:
                        self.pulsecond[portid].notify()

            if uselist and count == 4:
                with cond:
                    cond.notify()

            if not uselist and line[:len(BuildHAT.DONE)] == BuildHAT.DONE:
                def runit():
                    with cond:
                        cond.notify()
                t = Timer(8.0, runit)
                t.start()

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
                    q.put((callit, newdata))
                port.data = newdata
                with self.portcond[portid]:
                    self.portcond[portid].notify()
