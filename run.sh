#!/bin/bash
set -e
cd ${0%/*}

# 1. select source
if [ "${1}" == "" ]; then
    EXAMPLE_NAME="test"
else
    EXAMPLE_NAME=${1}
fi
echo -e "Example Name: ${EXAMPLE_NAME}\n"

# 2. clean, build
rm -rf *.exe
gcc -o ${EXAMPLE_NAME}.exe example/${EXAMPLE_NAME}.c    \
    -Isrc src/ble.c src/nameof.c                        \
    -Ibluez bluez/bluetooth.c bluez/hci.c

# 3. run, clean
sudo ./${EXAMPLE_NAME}.exe
rm -rf *.exe
