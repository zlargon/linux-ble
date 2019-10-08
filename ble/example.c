#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"

void wait(unsigned int sec) {
    printf("\nwait %u sec...", sec);
    sleep(3);
    puts("\n");
}

int main() {
    puts("\nhci_init");
    HCIDevice hci = {};
    hci_init(&hci);

    // scan BLE devices
    puts("\nhci_scan_ble");
    BLEDevice ble_list[20] = {};
    int ble_list_len = hci_scan_ble(&hci, ble_list, 20, 5000);
    if (ble_list_len == -1) {
        return -1;
    }

    // filter Crimson_ device
    BLEDevice * ble = NULL;
    for (int i = 0; i < ble_list_len; i++) {
        BLEDevice * tmp = ble_list + i;

        const char * prefix = "Crimson_";
        int ret = strncmp(tmp->name, prefix, strlen(prefix));
        if (ret == 0) {
            printf("%s - %s\n", tmp->addr, tmp->name);
            ble = tmp;
        }
    }
    if (ble == NULL) {
        puts("Cimrson Device is not found");
        return 0;
    }

    wait(3);

    // connect
    printf("ble_connect => %s (%s)\n", ble->addr, ble->name);
    ble_connect(ble);

    wait(3);

    // disconnect
    printf("ble_disconnect = %s (%s)\n", ble->addr, ble->name);
    ble_disconnect(ble);

    hci_close(&hci);
    return 0;
}
