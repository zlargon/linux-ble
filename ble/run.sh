#!/bin/bash

# clean
rm -rf *.exe

# build
gcc -o example.exe example.c ble.c -lbluetooth

# reopen bluetooth if occur error
# https://stackoverflow.com/a/23059924
# sudo hciconfig hci0 down
# sudo hciconfig hci0 up
# service bluetooth restart

# run
sudo ./example.exe
