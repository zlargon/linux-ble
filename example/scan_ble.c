#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"
#include "nameof.h"

#define SCAN_TIME       5000    // 5000 ms
#define SCAN_LIST_LEN   20

int main() {
    // 1. create hci
    puts("\nhci_init");
    HCIDevice hci = {};
    int ret = hci_init(&hci);
    if (ret == -1) {
        return 0;
    }

    // 2. hci scan BLE devices
    puts("\nhci_scan_ble");
    BLEDevice ble_list[SCAN_LIST_LEN] = {};
    int ble_list_len = hci_scan_ble(&hci, ble_list, SCAN_LIST_LEN, SCAN_TIME);
    if (ble_list_len == -1) {
        return 0;
    }

    // 3. show list
    puts("\nBLE Scan Result:");
    for (int i = 0; i < ble_list_len; i++) {
        BLEDevice * ble = ble_list + i;
        printf("%s - %s\n", ble->addr_s, ble->name);
    }

    // 4. close hci
    hci_close(&hci);
    return 0;
}
