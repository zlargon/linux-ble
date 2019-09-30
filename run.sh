#!/bin/bash
# sudo apt install libbluetooth-dev

# 1. clean
echo -e "\n[x] Clean"
rm -rvf *.exe

# 2. select source
if [ "${1}" == "" ]; then
    src="main.c"
else
    src=${1}
fi

# 3. build
echo -e "\n[x] Build ${src}"
gcc -o ${src}.exe ${src} -lbluetooth

# 4. run
echo -e "\n[x] Run ${src}"
./${src}.exe
