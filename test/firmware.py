"""Test uploading firmware"""

import time
import unittest
from multiprocessing import Process

from gpiozero import DigitalOutputDevice

RESET_GPIO_NUMBER = 4
BOOT0_GPIO_NUMBER = 22


def resethat():
    """Reset the HAT"""
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


def reboot():
    """Reboot hat"""
    from buildhat import Hat
    resethat()
    h = Hat(debug=True)
    print(h.get())


class TestFirmware(unittest.TestCase):
    """Test firmware uploading functions"""

    def test_upload(self):
        """Test uploading firmware"""
        for _ in range(200):
            p = Process(target=reboot)
            p.start()
            p.join()


if __name__ == '__main__':
    unittest.main()
