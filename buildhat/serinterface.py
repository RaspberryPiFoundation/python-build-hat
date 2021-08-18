import threading
from threading import Condition
import serial

CONNECTED = ": connected to active ID"
NOTCONNECTED = ": no device detected"
MOTOR_BIAS="bias .2"
MOTOR_PLIMIT="plimit .4"
PULSEDONE=": pulse done"
RAMPDONE=": ramp done"

class Device():
    def __init__(self, port, porto, buildhat):
        self.port = port
        self.porto = porto
        self.buildhat = buildhat
        self.callit = None
        self.FORMAT_SI = 0

    def get(self, ig):
        self.buildhat.write("port {} ; selonce 0\r".format(self.port).encode())
        # wait for data
        with self.buildhat.portcond[self.port]:
            self.buildhat.portcond[self.port].wait()
        return self.porto.data

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

class BuildHAT:
    
    def __init__(self, firmware, signature, version):
        self.cond = Condition()
        
        self.portcond = []
        self.pulsecond = []
        self.rampcond = []

        for i in range(4):
            self.portcond.append(Condition())
            self.pulsecond.append(Condition())
            self.rampcond.append(Condition())

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

    def write(self, data):
        self.ser.write(data)

    def loop(self, cond):
        count = 0
        while True:
            try:
                line = self.ser.readline().decode('utf-8')
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
                    

    
