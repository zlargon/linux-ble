#include <stdbool.h>

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

typedef struct {
    int dev_id;             // device id
    int dd;                 // device description
    struct hci_filter of;   // original filter
} HCIDevice;

typedef struct {
    char addr[18];
    char name[30];
    int8_t rssi;

    HCIDevice * hci;
    uint16_t handle;
} BLEDevice;

int hci_init(HCIDevice * hci);
int hci_close(HCIDevice * hci);
int hci_scan_ble(HCIDevice * hci, BLEDevice * ble_list, int ble_list_len, int scan_time);

int ble_connect(BLEDevice * ble);
int ble_disconnect(BLEDevice * ble);
