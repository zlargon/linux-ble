#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"
#include "nameof.h"

int main() {
    HCIDevice hci[10] = {};

    // init 10 hci
    for (int i = 0; i < 10; i++) {
        hci_init(&hci[i]);
    }

    // close all hci
    for (int i = 0; i < 10; i++) {
        hci_close(&hci[i]);
    }

    return 0;
}
