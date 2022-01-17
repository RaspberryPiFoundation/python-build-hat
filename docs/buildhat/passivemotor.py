from buildhat import PassiveMotor
import time

motor = PassiveMotor('A')

print("Start motor")
motor.start()
time.sleep(3)
print("Stop motor")
motor.stop()


