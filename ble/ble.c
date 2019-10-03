#include "ble.h"

#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>       // gettimeofday
#include <stdlib.h>         // exit
#include <sys/select.h>     // select, FD_SET, FD_ZERO
#include <unistd.h>         // read

// Internal Functions
static long long get_current_time();
static int ble_find_index_by_address(BLEInfo * list, size_t list_len, const char * address);
static int eir_parse_name(uint8_t *eir, size_t eir_len, char *output_name, size_t output_name_len);

// 1. ble_scan
int ble_scan(BLEInfo **ble_info_list, size_t ble_info_list_len, int scan_time) {
    int ret = 0;
    long long start_time = get_current_time();

    // 1. create BLE device
    int dev_id = hci_get_route(NULL);
    int dd = hci_open_dev(dev_id);          // device description
    if (dev_id < 0 || dd < 0) {
        perror("opening socket error");
        return -1;
    }

    // 2. save original filter
    struct hci_filter original_filter;
    hci_filter_clear(&original_filter);
    socklen_t original_filter_len = sizeof(original_filter);
    ret = getsockopt(dd, SOL_HCI, HCI_FILTER, &original_filter, &original_filter_len);
    if (ret < 0) {
        perror("getsockopt failed");
        return -1;
    }

    // 3. setup new filter
    struct hci_filter new_filter;
    hci_filter_clear(&new_filter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);       // HCI_EVENT_PKT
    hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);   // EVT_LE_META_EVENT
    ret = setsockopt(dd, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter));
    if (ret < 0) {
        perror("setsockopt failed");
        return -1;
    }

    // 4. always disable scan before set params
    ret = hci_le_set_scan_enable(
        dd,
        0x00,       // disable
        0x00,       // no filter
        10000       // timeout
    );
    // not to check return value

    // 5. set scan params
    ret = hci_le_set_scan_parameters(
        dd,
        0x01,               // type
        htobs(0x0010),      // interval
        htobs(0x0010),      // window
        LE_PUBLIC_ADDRESS,  // own_type
        0x00,               // no filter
        10000               // timeout
    );
    if (ret != 0) {
        perror("hci_le_set_scan_parameters");
        goto end;
    }

    // 6. start scanning
    ret = hci_le_set_scan_enable(
        dd,
        0x01,       // enable
        0x00,       // no filter
        10000       // timeout
    );
    if (ret != 0) {
        perror("hci_le_set_scan_enable");
        goto end;
    }

    // 7. use 'select' to recieve data from fd
    int counter = 0;
    for (;;) {
        // set read file descriptions
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(dd, &rfds);

        struct timeval tv = {
            .tv_sec = 1,
            .tv_usec = 0,
        };

        // select read fds
        ret = select(
            dd + 1,     // MAX fd + 1
            &rfds,      // read fds
            NULL,       // write fds
            NULL,       // except fds
            &tv         // timeout (1 sec)
        );

        if (ret == -1) {
            perror("select error");
            goto end;
        }

        // check scan timeout
        if (get_current_time() - start_time > scan_time) {
            // BLE scan is done
            ret = counter;
            goto end;
        }

        if (ret == 0) {
            printf("select timeout");
            continue;
        }

        // ret > 0
        unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 };
        int buff_size = read(dd, buf, sizeof(buf));
        if (buff_size == 0) {
            printf("read no data");
            continue;
        }

        // get body ptr and body size
        unsigned char *ptr = buf + (1 + HCI_EVENT_HDR_SIZE);
        buff_size -= (1 + HCI_EVENT_HDR_SIZE);

        // parse to meta
        evt_le_meta_event *meta = (evt_le_meta_event *) ptr;
        if (meta->subevent != EVT_LE_ADVERTISING_REPORT) {
            printf("meta->subevent (0x%02x) is not EVT_LE_ADVERTISING_REPORT (0x%02x)\n",
                meta->subevent,
                EVT_LE_ADVERTISING_REPORT
            );
            continue;
        }

        // parse ADV info
        le_advertising_info *adv_info = (le_advertising_info *) (meta->data + 1);

        // parse address and name
        BLEInfo info = {};
        ba2str(&(adv_info->bdaddr), info.address);
        ret = eir_parse_name(adv_info->data, adv_info->length, info.name, sizeof(info.name) - 1);
        if (ret == -1) {
            // no name
            continue;
        }

        // check duplicated
        ret = ble_find_index_by_address(*ble_info_list, counter, info.address);
        if (ret >= 0 || counter >= ble_info_list_len) {
            continue;
        }

        // add info to list
        memcpy(*ble_info_list + counter, &info, sizeof(BLEInfo));
        counter++;

        // debug: show the address and name
        printf("%s - %s\n", info.address, info.name);
    }

    ret = 0;
end:
    setsockopt(dd, SOL_HCI, HCI_FILTER, &original_filter, original_filter_len);
    return ret;
}


// Internal Functions

// 1. get_current_time
static long long get_current_time() {
    struct timeval time = {};
    int ret = gettimeofday(&time, NULL);
    if (ret == -1) {
        perror("gettimeofday failed");
        return -1;
    }
    return (long long) time.tv_sec * 1000 + (long long) time.tv_usec / 1000;
}

// 2. ble_find_index_by_address
static int ble_find_index_by_address(BLEInfo * list, size_t list_len, const char * address) {
    for (int i = 0; i < list_len; i++) {
        BLEInfo * info = list + i;

        int ret = memcmp(info->address, address, sizeof(info->address));
        if (ret == 0) {
            return i;
        }
    }
    return -1;  // not found
}

// 3. eir_parse_name
static int eir_parse_name(uint8_t *eir, size_t eir_len, char *output_name, size_t output_name_len) {
    size_t index = 0;
    while (index < eir_len) {
        uint8_t * ptr = eir + index;

        // check field len
        uint8_t field_len = ptr[0];
        if (field_len == 0 || index + field_len > eir_len) {
            return -1;
        }

        // check EIR type (bluez/src/eir.h)
        // EIR_NAME_SHORT    0x08
        // EIR_NAME_COMPLETE 0x09
        if (ptr[1] == 0x08 || ptr[1] == 0x09) {
            size_t name_len = field_len - 1;
            if (name_len > output_name_len) {
                // output_name_len is too short
                printf("The length of device name is %ld, but the output_name_len (%ld) is too short.\n", name_len, output_name_len);
                return -1;
            }

            // copy value to output+name
            memcpy(output_name, &ptr[2], name_len);
            return 0;
        }

        // update index
        index += field_len + 1;
    }
    return -1;
}
