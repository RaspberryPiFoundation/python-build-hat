from buildhat import Light
from time import sleep

light = Light('A')

light.on()
sleep(1)
light.off()