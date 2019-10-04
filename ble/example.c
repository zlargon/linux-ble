#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"

int main() {
    // char * address = "D0:F6:1D:50:9F:1A";

    HCIDevice hci = {};
    hci_init(&hci);

    // scan BLE devices
    puts("Scan");
    BLEInfo * ble_info_list = (BLEInfo *) calloc(20, sizeof(BLEInfo));
    int ble_info_list_len = hci_scan_ble(&hci, &ble_info_list, 20, 5000);
    if (ble_info_list_len == -1) {
        return -1;
    }

    // filter Crimson_ device
    char * ble_addr = NULL;
    for (int i = 0; i < ble_info_list_len; i++) {
        BLEInfo * info = ble_info_list + i;

        const char * prefix = "Crimson_";
        int ret = strncmp(info->name, prefix, strlen(prefix));
        if (ret == 0) {
            printf("%s - %s\n", info->addr, info->name);
            ble_addr = info->addr;
        }
    }

    printf("BLE device address: %s\n", ble_addr);

    // connect ble device
    hci_connect_ble(&hci, ble_addr);

    free(ble_info_list);
    hci_close(&hci);
    return 0;
}
