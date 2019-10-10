#!/bin/bash
set -e

# sudo apt install libbluetooth-dev
# https://stackoverflow.com/a/23059924

rm -rf *.exe
gcc -o scan-classic.exe scan-classic.c -lbluetooth
./scan-classic.exe
