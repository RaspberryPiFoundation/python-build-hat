import threading
from threading import Condition
import serial

CONNECTED = ": connected to active ID"
NOTCONNECTED = ": no device detected"
MOTOR_BIAS="bias .2"
MOTOR_PLIMIT="plimit .4"

class Device():
    def __init__(self, port, buildhat):
        self.port = port
        self.buildhat = buildhat
        self.callit = None
    
    def mode(self, modev):
        if isinstance(modev, list):
            modestr = ""
            for t in modev:
                modestr += "{} {} ".format(t[0],t[1])
            self.buildhat.write("port {} ; combi 0 {}\r".format(self.port,modestr).encode())
    
    def callback(self, func):
        if func is not None:
            self.buildhat.write("port {} ; select 0\r".format(self.port).encode())
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
        print(newpos , curpos, speed)
        cmd = "port {} ; combi 0 1 0 2 0 3 0 ; select 0 ; {} ; {} ; pid 0 0 1 s4 0.0027777778 0 5 0 .1 3 ; set ramp {} {} {} 0\r".format(self.port, MOTOR_BIAS, MOTOR_PLIMIT, curpos, 
                                                                                                                    newpos, (newpos - curpos) / speed).encode()
        print(cmd)
        #self.buildhat.write(cmd);

    def pwm(self, pwmv):
        print(pwmv)
        self.buildhat.write("port {} ; pwm ; set {}\r".format(self.port,pwmv/100.0).encode())

    def float(self):
        self.pwm(0)

class Port:
    def __init__(self, port, buildhat):
        self.data = []
        self.port = port
        self.buildhat = buildhat
        self.device = Device(port, self.buildhat)    
        self.motor = InternalMotor(port, self, self.buildhat)

    def info(self):    
        return {'type' : self.typeid}

class Ports:
    def __init__(self, buildhat):
        self.A = Port(0,buildhat)
        self.B = Port(1,buildhat)
        self.C = Port(2,buildhat)
        self.D = Port(3,buildhat)

class BuildHAT:
    
    def __init__(self, firmware, signature, version):
        self.cond = Condition()
        
        self.portcond = []
        for i in range(4):
            self.portcond.append(Condition())

        self.ser = serial.Serial('/dev/serial0',115200)
        self.port = Ports(self)
        th = threading.Thread(target=self.loop, args=(self.cond, ))
        th.daemon = True
        th.start()

        self.write(b"port 0 ; select ; port 1 ; select ; port 2 ; select ; port 3 ; select ; echo 0\r")
        self.write(b"list\r")

        # Wait for initialisation to finish
        with self.cond:
            self.cond.wait()
            print("wait fin")

    def write(self, data):
        print(data)
        self.ser.write(data)

    def loop(self, cond):
        count = 0
        while True:
            try:
                line = self.ser.readline().decode('utf-8')
                if line[0] == "P" and line[2] == ":":
                    portid = int(line[1])
                    print(portid)
                    port = getattr(self.port,chr(ord('A')+portid))
                    if line[2:2+len(CONNECTED)] == CONNECTED:
                        typeid = int(line[2+len(CONNECTED):],16)
                        count += 1
                        port.typeid = typeid
                    if line[2:2+len(NOTCONNECTED)] == NOTCONNECTED:
                        typeid = -1
                        count += 1
                        port.typeid = typeid
                    if count == 4:
                        with cond:
                            cond.notify()
                if line[0] == "P" and line[2] == "C":
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

            except serial.SerialException:
                print("err")
                pass
                    

    
