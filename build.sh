#!/bin/bash

python3 -m venv hat_env 
source hat_env/bin/activate 
USE_DUMMY_I2C=1 ./setup.py build 
USE_DUMMY_I2C=1 ./setup.py install