#include <stdbool.h>

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define HCI_MAX_CONN  20

typedef struct {
    int dev_id;             // device id
    int dd;                 // device description
    struct hci_filter of;   // original filter

    // connect list
    struct {
        uint16_t dev_id;    // for ioctl HCIGETCONNLIST
        uint16_t num;
        struct hci_conn_info list[HCI_MAX_CONN];
    } conn;

} HCIDevice;

typedef struct {
    char name[30];
    int8_t rssi;

    // address
    bdaddr_t addr;
    char addr_s[18];
    uint8_t addr_type;

    HCIDevice * hci;
    uint16_t handle;
} BLEDevice;

int hci_init(HCIDevice * hci);
int hci_close(HCIDevice * hci);

int hci_scan_ble(HCIDevice * hci, BLEDevice * ble_list, int ble_list_len, int scan_time);
int hci_update_conn_list(HCIDevice * hci);

int ble_connect(BLEDevice * ble);
int ble_disconnect(BLEDevice * ble);
