#!/bin/bash

# sudo apt install libbluetooth-dev

rm -rf *.exe

# build
gcc -o main.exe main.c -lbluetooth

# run executable
./main.exe
