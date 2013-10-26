#!/bin/sh

while true
	do cat /dev/urandom | nc 127.0.0.1 1234 &
	sleep 1
done
