"""Example turning on/off LED lights"""

from time import sleep

from buildhat import Light

light = Light('A')

light.on()
sleep(1)
light.off()
