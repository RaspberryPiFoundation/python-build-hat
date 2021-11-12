#!/bin/bash

rm "/tmp/test"

while [[ 1 ]];
do
	rm /tmp/serial.txt

	./loadingtest.py
		
	if [ -f "/tmp/test" ]; then
		break
	fi
	sleep 1
done

cat /tmp/serial.txt
