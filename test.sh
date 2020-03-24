#!/bin/bash

source hat_env/bin/activate 

pip3 install psutil

python3 ./test/test_manual.py test/resources/FakeHat