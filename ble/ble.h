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
    int dev_id;             // device id
    int dd;                 // device description
    struct hci_filter of;   // original filter
} HCIDevice;

int hci_init(HCIDevice * hci);
int hci_close(HCIDevice * hci);
int hci_scan_ble(HCIDevice * hci, BLEInfo ** ble_info_list, int ble_info_list_len, int scan_time);
int hci_connect_ble(HCIDevice * hci, const char * ble_addr);
