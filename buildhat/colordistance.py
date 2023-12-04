"""Color distance sensor handling functionality"""

import math
from collections import deque
from threading import Condition

from .devices import Device


class ColorDistanceSensor(Device):
    """Color Distance sensor

    :param port: Port of device
    :raises DeviceError: Occurs if there is no colordistance sensor attached to port
    """

    def __init__(self, port):
        """
        Initialise color distance sensor

        :param port: Port of device
        """
        super().__init__(port)
        self.on()
        self.mode(6)
        self.avg_reads = 4
        self._old_color = None
        self._ir_channel = 0x0
        self._ir_address = 0x0
        self._ir_toggle = 0x0

    def segment_color(self, r, g, b):
        """Return the color name from HSV

        :param r: Red
        :param g: Green
        :param b: Blue
        :return: Name of the color as a string
        :rtype: str
        """
        table = [("black", (0, 0, 0)),
                 ("violet", (127, 0, 255)),
                 ("blue", (0, 0, 255)),
                 ("cyan", (0, 183, 235)),
                 ("green", (0, 128, 0)),
                 ("yellow", (255, 255, 0)),
                 ("red", (255, 0, 0)),
                 ("white", (255, 255, 255))]
        near = ""
        euc = math.inf
        for itm in table:
            cur = math.sqrt((r - itm[1][0]) ** 2 + (g - itm[1][1]) ** 2 + (b - itm[1][2]) ** 2)
            if cur < euc:
                near = itm[0]
                euc = cur
        return near

    def rgb_to_hsv(self, r, g, b):
        """Convert RGB to HSV

        Based on https://www.rapidtables.com/convert/color/rgb-to-hsv.html algorithm

        :param r: Red
        :param g: Green
        :param b: Blue
        :return: HSV representation of color
        :rtype: tuple
        """
        r, g, b = r / 255.0, g / 255.0, b / 255.0
        cmax = max(r, g, b)
        cmin = min(r, g, b)
        delt = cmax - cmin
        if cmax == cmin:
            h = 0
        elif cmax == r:
            h = 60 * (((g - b) / delt) % 6)
        elif cmax == g:
            h = 60 * ((((b - r) / delt)) + 2)
        elif cmax == b:
            h = 60 * ((((r - g) / delt)) + 4)
        if cmax == 0:
            s = 0
        else:
            s = delt / cmax
        v = cmax
        return int(h), int(s * 100), int(v * 100)

    def get_color(self):
        """Return the color

        :return: Name of the color as a string
        :rtype: str
        """
        r, g, b = self.get_color_rgb()
        return self.segment_color(r, g, b)

    def get_ambient_light(self):
        """Return the ambient light

        :return: Ambient light
        :rtype: int
        """
        self.mode(4)
        readings = []
        for _ in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings) / len(readings))

    def get_reflected_light(self):
        """Return the reflected light

        :return: Reflected light
        :rtype: int
        """
        self.mode(3)
        readings = []
        for _ in range(self.avg_reads):
            readings.append(self.get()[0])
        return int(sum(readings) / len(readings))

    def get_distance(self):
        """Return the distance

        :return: Distance
        :rtype: int
        """
        self.mode(1)
        distance = self.get()[0]
        return distance

    def _clamp(self, val, small, large):
        return max(small, min(val, large))

    def _avgrgb(self, reads):
        readings = []
        for read in reads:
            read = [int((self._clamp(read[0], 0, 400) / 400) * 255),
                    int((self._clamp(read[1], 0, 400) / 400) * 255),
                    int((self._clamp(read[2], 0, 400) / 400) * 255)]
            readings.append(read)
        rgb = []
        for i in range(3):
            rgb.append(int(sum([rgb[i] for rgb in readings]) / len(readings)))
        return rgb

    def get_color_rgb(self):
        """Return the color

        :return: RGBI representation
        :rtype: list
        """
        self.mode(6)
        reads = []
        for _ in range(self.avg_reads):
            reads.append(self.get())
        return self._avgrgb(reads)

    def _cb_handle(self, lst):
        self._data.append(lst)
        if len(self._data) == self.avg_reads:
            r, g, b = self._avgrgb(self._data)
            seg = self.segment_color(r, g, b)
            if self._cmp(seg, self._color):
                with self._cond:
                    self._old_color = seg
                    self._cond.notify()

    def wait_until_color(self, color):
        """Wait until specific color

        :param color: Color to look for
        """
        self.mode(6)
        self._cond = Condition()
        self._data = deque(maxlen=self.avg_reads)
        self._color = color
        self._cmp = lambda x, y: x == y
        self.callback(self._cb_handle)
        with self._cond:
            self._cond.wait()
        self.callback(None)

    def wait_for_new_color(self):
        """Wait for new color or returns immediately if first call

        :return: Name of the color as a string
        :rtype: str
        """
        self.mode(6)
        if self._old_color is None:
            self._old_color = self.get_color()
            return self._old_color
        self._cond = Condition()
        self._data = deque(maxlen=self.avg_reads)
        self._color = self._old_color
        self._cmp = lambda x, y: x != y
        self.callback(self._cb_handle)
        with self._cond:
            self._cond.wait()
        self.callback(None)
        return self._old_color

    @property
    def ir_channel(self):
        """Get the IR channel for message transmission"""
        return self._ir_channel

    @ir_channel.setter
    def ir_channel(self, channel=1):
        """
        Set the IR channel for RC Tx

        :param channel: 1-4 indicating the selected IR channel on the reciever
        """
        check_chan = channel
        if check_chan > 4:
            check_chan = 4
        elif check_chan < 1:
            check_chan = 1
        # Internally: 0-3
        self._ir_channel = int(check_chan) - 1

    @property
    def ir_address(self):
        """IR Address space of 0x0 for default PoweredUp or 0x1 for extra space"""
        return self._ir_address

    def toggle_ir_toggle(self):
        """Toggle the IR toggle bit"""
        # IYKYK, because the RC documents are not clear
        if self._ir_toggle:
            self._ir_toggle = 0x0
        else:
            self._ir_toggle = 0x1
        return self._ir_toggle

    def send_ir_sop(self, port, mode):
        """
        Send an IR message via Power Functions RC Protocol in Single Output PWM mode

        PF IR RC Protocol documented at https://www.philohome.com/pf/pf.htm

        Port B is blue

        Valid values for mode are:
        0x0: Float output
        0x1: Forward/Clockwise at speed 1
        0x2: Forward/Clockwise at speed 2
        0x3: Forward/Clockwise at speed 3
        0x4: Forward/Clockwise at speed 4
        0x5: Forward/Clockwise at speed 5
        0x6: Forward/Clockwise at speed 6
        0x7: Forward/Clockwise at speed 7
        0x8: Brake (then float v1.20)
        0x9: Backwards/Counterclockwise at speed 7
        0xA: Backwards/Counterclockwise at speed 6
        0xB: Backwards/Counterclockwise at speed 5
        0xC: Backwards/Counterclockwise at speed 4
        0xD: Backwards/Counterclockwise at speed 3
        0xE: Backwards/Counterclockwise at speed 2
        0xF: Backwards/Counterclockwise at speed 1

        :param port: 'A' or 'B'
        :param mode: 0-15 indicating the port's mode to set
        """
        escape_modeselect = 0x0
        escape = escape_modeselect

        ir_mode_single_output = 0x4
        ir_mode = ir_mode_single_output

        so_mode_pwm = 0x0
        so_mode = so_mode_pwm

        output_port_a = 0x0
        output_port_b = 0x1

        output_port = None
        if port == 'A' or port == 'a':
            output_port = output_port_a
        elif port == 'B' or port == 'b':
            output_port = output_port_b
        else:
            return False

        ir_mode = ir_mode | (so_mode << 1) | output_port

        nibble1 = (self._ir_toggle << 3) | (escape << 2) | self._ir_channel
        nibble2 = (self._ir_address << 3) | ir_mode

        # Mode range checked here
        return self._send_ir_nibbles(nibble1, nibble2, mode)

    def send_ir_socstid(self, port, mode):
        """
        Send an IR message via Power Functions RC Protocol in Single Output Clear/Set/Toggle/Increment/Decrement mode

        PF IR RC Protocol documented at https://www.philohome.com/pf/pf.htm

        Valid values for mode are:
        0x0: Toggle full Clockwise/Forward (Stop to Clockwise, Clockwise to Stop, Counterclockwise to Clockwise)
        0x1: Toggle direction
        0x2: Increment numerical PWM
        0x3: Decrement numerical PWM
        0x4: Increment PWM
        0x5: Decrement PWM
        0x6: Full Clockwise/Forward
        0x7: Full Counterclockwise/Backward
        0x8: Toggle full (defaults to Forward, first)
        0x9: Clear C1 (C1 to High)
        0xA: Set C1 (C1 to Low)
        0xB: Toggle C1
        0xC: Clear C2 (C2 to High)
        0xD: Set C2 (C2 to Low)
        0xE: Toggle C2
        0xF: Toggle full Counterclockwise/Backward (Stop to Clockwise, Counterclockwise to Stop, Clockwise to Counterclockwise)

        :param port: 'A' or 'B'
        :param mode: 0-15 indicating the port's mode to set
        """
        escape_modeselect = 0x0
        escape = escape_modeselect

        ir_mode_single_output = 0x4
        ir_mode = ir_mode_single_output

        so_mode_cstid = 0x1
        so_mode = so_mode_cstid

        output_port_a = 0x0
        output_port_b = 0x1

        output_port = None
        if port == 'A' or port == 'a':
            output_port = output_port_a
        elif port == 'B' or port == 'b':
            output_port = output_port_b
        else:
            return False

        ir_mode = ir_mode | (so_mode << 1) | output_port

        nibble1 = (self._ir_toggle << 3) | (escape << 2) | self._ir_channel
        nibble2 = (self._ir_address << 3) | ir_mode

        # Mode range checked here
        return self._send_ir_nibbles(nibble1, nibble2, mode)

    def send_ir_combo_pwm(self, port_b_mode, port_a_mode):
        """
        Send an IR message via Power Functions RC Protocol in Combo PWM mode

        PF IR RC Protocol documented at https://www.philohome.com/pf/pf.htm

        Valid values for the modes are:
        0x0 Float
        0x1 PWM Forward step 1
        0x2 PWM Forward step 2
        0x3 PWM Forward step 3
        0x4 PWM Forward step 4
        0x5 PWM Forward step 5
        0x6 PWM Forward step 6
        0x7 PWM Forward step 7
        0x8 Brake (then float v1.20)
        0x9 PWM Backward step 7
        0xA PWM Backward step 6
        0xB PWM Backward step 5
        0xC PWM Backward step 4
        0xD PWM Backward step 3
        0xE PWM Backward step 2
        0xF PWM Backward step 1

        :param port_b_mode: 0-15 indicating the command to send to port B
        :param port_a_mode: 0-15 indicating the command to send to port A
        """
        escape_combo_pwm = 0x1
        escape = escape_combo_pwm

        nibble1 = (self._ir_toggle << 3) | (escape << 2) | self._ir_channel

        # Port modes are range checked here
        return self._send_ir_nibbles(nibble1, port_b_mode, port_a_mode)

    def send_ir_combo_direct(self, port_b_output, port_a_output):
        """
        Send an IR message via Power Functions RC Protocol in Combo Direct mode

        PF IR RC Protocol documented at https://www.philohome.com/pf/pf.htm

        Valid values for the output variables are:
        0x0: Float output
        0x1: Clockwise/Forward
        0x2: Counterclockwise/Backwards
        0x3: Brake then float

        :param port_b_output: 0-3 indicating the output to send to port B
        :param port_a_output: 0-3 indicating the output to send to port A
        """
        escape_modeselect = 0x0
        escape = escape_modeselect

        ir_mode_combo_direct = 0x1
        ir_mode = ir_mode_combo_direct

        nibble1 = (self._ir_toggle << 3) | (escape << 2) | self._ir_channel
        nibble2 = (self._ir_address << 3) | ir_mode

        if port_b_output > 0x3 or port_a_output > 0x3:
            return False
        if port_b_output < 0x0 or port_a_output < 0x0:
            return False

        nibble3 = (port_b_output << 2) | port_a_output

        return self._send_ir_nibbles(nibble1, nibble2, nibble3)

    def send_ir_extended(self, mode):
        """
        Send an IR message via Power Functions RC Protocol in Extended mode

        PF IR RC Protocol documented at https://www.philohome.com/pf/pf.htm

        Valid values for the mode are:
        0x0: Brake Port A (timeout)
        0x1: Increment Speed on Port A
        0x2: Decrement Speed on Port A

        0x4: Toggle Forward/Clockwise/Float on Port B

        0x6: Toggle Address bit
        0x7: Align toggle bit

        :param mode: 0-2,4,6-7
        """
        escape_modeselect = 0x0
        escape = escape_modeselect

        ir_mode_extended = 0x0
        ir_mode = ir_mode_extended

        nibble1 = (self._ir_toggle << 3) | (escape << 2) | self._ir_channel
        nibble2 = (self._ir_address << 3) | ir_mode

        if mode < 0x0 or mode == 0x3 or mode == 0x5 or mode > 0x7:
            return False

        return self._send_ir_nibbles(nibble1, nibble2, mode)

    def send_ir_single_pin(self, port, pin, mode, timeout):
        """
        Send an IR message via Power Functions RC Protocol in Single Pin mode

        PF IR RC Protocol documented at https://www.philohome.com/pf/pf.htm

        Valid values for the mode are:
        0x0: No-op
        0x1: Clear
        0x2: Set
        0x3: Toggle

        Note: The unlabeled IR receiver (vs the one labeled V2) has a "firmware bug in Single Pin mode"
        https://www.philohome.com/pfrec/pfrec.htm

        :param port: 'A' or 'B'
        :param pin: 1 or 2
        :param mode: 0-3 indicating the pin's mode to set
        :param timeout: True or False
        """
        escape_mode = 0x0
        escape = escape_mode

        ir_mode_single_continuous = 0x2
        ir_mode_single_timeout = 0x3
        ir_mode = None
        if timeout:
            ir_mode = ir_mode_single_timeout
        else:
            ir_mode = ir_mode_single_continuous

        output_port_a = 0x0
        output_port_b = 0x1

        output_port = None
        if port == 'A' or port == 'a':
            output_port = output_port_a
        elif port == 'B' or port == 'b':
            output_port = output_port_b
        else:
            return False

        if pin != 1 and pin != 2:
            return False
        pin_value = pin - 1

        if mode > 0x3 or mode < 0x0:
            return False

        nibble1 = (self._ir_toggle << 3) | (escape << 2) | self._ir_channel
        nibble2 = (self._ir_address << 3) | ir_mode
        nibble3 = (output_port << 3) | (pin_value << 3) | mode

        return self._send_ir_nibbles(nibble1, nibble2, nibble3)

    def _send_ir_nibbles(self, nibble1, nibble2, nibble3):

        #  M7 IR Tx SI = N/A
        #    format count=1 type=1 chars=5 dp=0
        #    RAW: 00000000 0000FFFF    PCT: 00000000 00000064    SI: 00000000 0000FFFF

        mode = 7
        self.mode(mode)

        # The upper bits of data[2] are ignored
        if nibble1 > 0xF or nibble2 > 0xF or nibble3 > 0xF:
            return False
        if nibble1 < 0x0 or nibble2 < 0x0 or nibble3 < 0x0:
            return False

        byte_two = (nibble2 << 4) | nibble3

        data = bytearray(3)
        data[0] = (0xc << 4) | mode
        data[1] = byte_two
        data[2] = nibble1

        # print(" ".join('{:04b}'.format(nibble1)))
        # print(" ".join('{:04b}'.format(nibble2)))
        # print(" ".join('{:04b}'.format(nibble3)))
        # print(" ".join('{:08b}'.format(n) for n in data))

        self._write1(data)
        return True

    def on(self):
        """Turn on the sensor and LED"""
        self.reverse()
