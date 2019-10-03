#!/bin/bash

rm -rf *.exe

gcc -o example.exe example.c ble.c -lbluetooth

sudo ./example.exe
