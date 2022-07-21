"""Test uploading firmware"""

import time
import unittest
from multiprocessing import Process, Queue
from queue import Empty

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


def reboot(exc):
    """Reboot hat and load firmware

    :param exc: Queue to pass exceptions
    """
    try:
        from buildhat import Hat
        resethat()
        h = Hat(debug=True)
        print(h.get())
    except Exception as e:
        exc.put(e)


class TestFirmware(unittest.TestCase):
    """Test firmware uploading functions"""

    def test_upload(self):
        """Test upload firmware

        :raises exc.get: Raised if exception in subprocess
        """
        for _ in range(2):
            exc = Queue()
            p = Process(target=reboot, args=(exc,))
            p.start()
            p.join()
            try:
                raise exc.get(False)
            except Empty:
                pass


if __name__ == '__main__':
    unittest.main()
