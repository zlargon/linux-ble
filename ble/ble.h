#include <stdbool.h>

typedef struct {
    char addr[18];
    char name[30];
} BLEInfo;

// 1. ble_scan
int ble_scan(BLEInfo **ble_info_list, int ble_info_list_len, int scan_time);
