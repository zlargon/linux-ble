#include <stdbool.h>

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

typedef struct {
    char address[18];
    char name[30];
} BLEInfo;

// 1. ble_scan
int ble_scan(BLEInfo **ble_info_list, size_t ble_info_list_len, int scan_time);
