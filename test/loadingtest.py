#!/usr/bin/python3

import datetime
from buildhat import Hat

try:
    print("{} Starting".format(datetime.datetime.now().strftime("%H:%M:%S")))
    h = Hat()
    print(h.get())
    print("Fin")
except Exception as err:
    print(err)
    with open('/tmp/test', 'w'): 
        pass
