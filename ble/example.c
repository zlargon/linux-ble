#include <stdio.h>
#include <stdlib.h>
#include "ble.h"

int main() {
    // scan BLE devices
    BLEInfo * ble_info_list = (BLEInfo *) calloc(20, sizeof(BLEInfo));
    int ble_info_list_len = ble_scan(&ble_info_list, 20, 5000);
    if (ble_info_list_len == -1) {
        return -1;
    }

    free(ble_info_list);
    return 0;
}
