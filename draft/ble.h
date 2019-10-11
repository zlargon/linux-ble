#include <stdbool.h>

// bluez
#include <bluetooth.h>
#include <hci.h>

typedef struct {
    char name[30];
    int8_t rssi;

    // address
    bdaddr_t    addr;
    char        addr_s[18];
    uint8_t     addr_type;

    // connect info
    struct hci_conn_info conn;

    // hci info
    struct {
        int sock;
        struct hci_filter   of;   // original filter
        struct hci_dev_info info;
    } hci;

} BLEDevice;

// Scan/Discover API
int ble_scan_enable(bool enable);
int ble_scan_filter_set(BLEDevice * ble, bool (*scan_filter) (BLEDevice * ble, bool is_repeated));
int ble_scan_list_get(BLEDevice * ble_list, int ble_list_len);
int ble_on_discover_callback_set(BLEDevice * ble, int (*on_discover) (BLEDevice * ble));

// Connect/Disconnect API
int ble_connect(BLEDevice * ble);       // blocking
int ble_disconnect(BLEDevice * ble);    // blocking
int ble_connect_list_get(BLEDevice * ble_list, int ble_list_len);
int ble_on_disconnect_callback_set(BLEDevice * ble, int (*on_disconnected) (BLEDevice * ble));

// Data Send/Receive API
int ble_send(BLEDevice * ble, uint8_t * data, uint8_t data_len);    // blocking
int ble_on_receive_callback_set(BLEDevice * ble, int (*on_receive) (BLEDevice * ble, uint8_t * data, uint8_t data_len));

// Loop Start/Stop API
int ble_loop(bool start);
