#!/bin/bash
set -e

# sudo apt install libbluetooth-dev

# 1. clean
echo -e "[x] Clean"
rm -rf *.exe

# 2. select source
if [ "${1}" == "" ]; then
    src="main.c"
else
    src=${1}
fi

# 3. build
echo -e "[x] Build ${src}"
gcc -o ${src}.exe ${src} -lbluetooth

# 4. run
echo -e "[x] Run ${src}\n"
./${src}.exe
