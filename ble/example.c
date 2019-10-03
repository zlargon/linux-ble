#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ble.h"

int main() {
    // scan BLE devices
    BLEInfo * ble_info_list = (BLEInfo *) calloc(20, sizeof(BLEInfo));
    int ble_info_list_len = ble_scan(&ble_info_list, 20, 5000);
    if (ble_info_list_len == -1) {
        return -1;
    }

    // filter Crimson_ device
    for (int i = 0; i < ble_info_list_len; i++) {
        BLEInfo * info = ble_info_list + i;

        const char * prefix = "Crimson_";
        int ret = strncmp(info->name, prefix, sizeof(prefix));
        if (ret == 0) {
            printf("%s - %s\n", info->address, info->name);
        }
    }



    free(ble_info_list);
    return 0;
}
