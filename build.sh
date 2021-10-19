#!/bin/bash

python3 -m venv hat_env 
source hat_env/bin/activate 
./setup.py build
./setup.py install
