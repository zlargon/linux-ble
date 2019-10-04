#!/bin/bash
set -e

# reopen bluetooth if occur error
# https://stackoverflow.com/a/23059924
sudo hciconfig hci0 down
sudo hciconfig hci0 up
# service bluetooth restart
