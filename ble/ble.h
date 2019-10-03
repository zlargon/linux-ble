#include <stdbool.h>

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

typedef struct {
    char addr[18];
    char name[30];
} BLEInfo;

typedef struct {
    int dev_id;
    int dd;                 // device description
    struct hci_filter of;   // original filter
} BLEDevice;

int ble_init(BLEDevice *ble);
int ble_close(BLEDevice *ble);
int ble_scan(BLEDevice * ble, BLEInfo **ble_info_list, int ble_info_list_len, int scan_time);
