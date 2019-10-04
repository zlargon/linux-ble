#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"

int main() {
    puts("\nhci_init");
    HCIDevice hci = {};
    hci_init(&hci);

    // scan BLE devices
    puts("\nhci_scan_ble");
    BLEDevice * ble_list = (BLEDevice *) calloc(20, sizeof(BLEDevice));
    int ble_list_len = hci_scan_ble(&hci, &ble_list, 20, 5000);
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

    // show device
    printf("\n  ble addr: %s\n", ble->addr);
    printf("  ble name: %s\n",   ble->name);
    printf("hci dev_id: %d\n",   ble->hci->dev_id);
    printf("    hci dd: %d\n",   ble->hci->dd);

    free(ble_list);
    hci_close(&hci);
    return 0;
}
