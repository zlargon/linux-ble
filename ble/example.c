#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // sleep
#include "ble.h"
#include "nameof.h"

void wait(unsigned int sec) {
    printf("\nwait %u sec...", sec);
    sleep(3);
    puts("\n");
}

int main() {
    // 1. create hci
    puts("\nhci_init");
    HCIDevice hci = {};
    hci_init(&hci);

    // 2. hci scan BLE devices
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
            printf("%s - %s\n", tmp->addr_s, tmp->name);
            ble = tmp;
        }
    }
    if (ble == NULL) {
        puts("Cimrson Device is not found");
        return 0;
    }

    // 3. connect
    wait(3);
    printf("ble_connect => %s (%s)\n", ble->addr_s, ble->name);
    ble_connect(ble);

    // 4. show connected list
    wait(3);
    puts("hci_update_conn_list");
    hci_update_conn_list(&hci);
    for (int i = 0; i < hci.conn.num; i++) {
        struct hci_conn_info * info = hci.conn.list + i;
        char addr[18] = {};
        ba2str(&(info->bdaddr), addr);
        printf("%s\n", addr);
        printf("    baseband  = %s\n",   nameof_baseband(info->type));
        printf("    link_mode = %s\n",   nameof_link_mode(info->link_mode));
        printf("    handle    = %d\n",   info->handle);
        printf("    state     = %s\n\n", nameof_conn_state(info->state));
    }

    // 5. disconnect
    wait(3);
    printf("ble_disconnect = %s (%s)\n", ble->addr_s, ble->name);
    ble_disconnect(ble);

    // 6. close hci
    hci_close(&hci);
    return 0;
}
