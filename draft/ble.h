#include <stdbool.h>            // bool
#include "bluez/bluetooth.h"    // bdaddr_t
#include "bluez/hci.h"          // hci_*

// BLE Device
typedef struct BLEDevice {
    char    name[30];
    char    address[18];
    int8_t  rssi;

    // when
    struct {
        long long discovered;
        long long connected;
        long long disconnected;
    } when;

    // address info
    uint8_t   addressType;
    bdaddr_t  bdaddr;

    // connect info
    struct hci_conn_info conn;

    // hci info
    struct {
        int sock;
        struct hci_filter   of;   // original filter
        struct hci_dev_info info;
    } hci;

    // callbacks
    int (*onDisconnected) (struct BLEDevice * ble, int reason);
    int (*onDataReceived) (struct BLEDevice * ble, uint8_t * data, uint8_t dataLen);
} BLEDevice;

// BLE Device List
#define BLE_DEVICE_LIST_MAX_NUM 30
typedef struct {
    int num;
    BLEDevice ble[BLE_DEVICE_LIST_MAX_NUM];
} BLEDeviceList;

// init
int ble_init();

// Scanner
int ble_scanner_enable(bool option);
int ble_scanner_getScanList(BLEDeviceList * list);

typedef int (*ble_discoveredCallback) (BLEDevice * ble, bool isDuplicated);
int ble_scanner_setDiscoverCallback(ble_discoveredCallback cb);

// BLE Device (blocking)
int ble_device_connect(BLEDevice * ble);
int ble_device_disconnect(BLEDevice * ble);
int ble_device_sendData(BLEDevice * ble, uint8_t * data, uint8_t dataLen);
