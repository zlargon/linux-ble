#!/bin/bash

# clean
rm -rf *.exe

# build
gcc -o example.exe example.c ble.c adv.c -lbluetooth

# run
sudo ./example.exe
