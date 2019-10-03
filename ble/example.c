#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"

int main() {
    // char * address = "D0:F6:1D:50:9F:1A";

    BLEDevice ble = {};
    ble_init(&ble);

    // scan BLE devices
    puts("Scan");
    BLEInfo * ble_info_list = (BLEInfo *) calloc(20, sizeof(BLEInfo));
    int ble_info_list_len = ble_scan(&ble, &ble_info_list, 20, 5000);
    if (ble_info_list_len == -1) {
        return -1;
    }

    // filter Crimson_ device
    char * address = NULL;
    for (int i = 0; i < ble_info_list_len; i++) {
        BLEInfo * info = ble_info_list + i;

        const char * prefix = "Crimson_";
        int ret = strncmp(info->name, prefix, strlen(prefix));
        if (ret == 0) {
            printf("%s - %s\n", info->addr, info->name);
            address = info->addr;
        }
    }

    printf("address: %s\n", address);

    free(ble_info_list);
    ble_close(&ble);
    return 0;
}
