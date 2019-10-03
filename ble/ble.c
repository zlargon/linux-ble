#include "ble.h"

#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>       // gettimeofday
#include <stdlib.h>         // exit
#include <sys/select.h>     // select, FD_SET, FD_ZERO
#include <unistd.h>         // read
#include <sys/ioctl.h>

// Internal Functions
static long long get_current_time();
static int ble_find_index_by_address(BLEInfo * list, size_t list_len, const char * address);
static int eir_parse_name(uint8_t *eir, size_t eir_len, char *output_name, size_t output_name_len);

// 1. ble_init
int ble_init(BLEDevice *ble) {
    // 1. create BLE device
    ble->dev_id = hci_get_route(NULL);
    ble->dd = hci_open_dev(ble->dev_id);          // device description
    if (ble->dev_id < 0 || ble->dd < 0) {
        perror("opening socket error");
        return -1;
    }

    printf("dev_id = %d\n", ble->dev_id);
    printf("dd = %d\n", ble->dd);

    // 2. save original filter
    hci_filter_clear(&ble->of);
    socklen_t of_len = sizeof(ble->of);
    int ret = getsockopt(ble->dd, SOL_HCI, HCI_FILTER, &ble->of, &of_len);
    if (ret < 0) {
        perror("getsockopt failed");
        return -1;
    }

    // 3. set new filter
    struct hci_filter nf;
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);       // HCI_EVENT_PKT
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);   // EVT_LE_META_EVENT
    ret = setsockopt(ble->dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));
    if (ret < 0) {
        perror("setsockopt failed");
        return -1;
    }

    return 0;
}

// 2. ble_close
int ble_close(BLEDevice *ble) {
    setsockopt(ble->dd, SOL_HCI, HCI_FILTER, &(ble->of), sizeof(ble->of));
    hci_close_dev(ble->dd);
    memset(ble, 0, sizeof(BLEDevice));
    return 0;
}

// 3. ble_scan
int ble_scan(BLEDevice *ble, BLEInfo **ble_info_list, int ble_info_list_len, int scan_time) {
    long long start_time = get_current_time();

    // 1. always disable scan before set params
    int ret = hci_le_set_scan_enable(
        ble->dd,
        0x00,       // disable
        0x00,       // no filter
        10000       // timeout
    );
    // not to check return value

    // 2. set scan params
    ret = hci_le_set_scan_parameters(
        ble->dd,
        0x01,               // type
        htobs(0x0010),      // interval
        htobs(0x0010),      // window
        LE_PUBLIC_ADDRESS,  // own_type
        0x00,               // no filter
        10000               // timeout
    );
    if (ret != 0) {
        perror("hci_le_set_scan_parameters");
        return -1;
    }

    // 3. start scanning
    ret = hci_le_set_scan_enable(
        ble->dd,
        0x01,       // enable
        0x00,       // no filter
        10000       // timeout
    );
    if (ret != 0) {
        perror("hci_le_set_scan_enable");
        return -1;
    }

    // 4. use 'select' to recieve data from fd
    int counter = 0;
    for (;;) {
        // set read file descriptions
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(ble->dd, &rfds);

        struct timeval tv = {
            .tv_sec = 1,
            .tv_usec = 0,
        };

        // select read fds
        ret = select(
            ble->dd + 1,    // MAX fd + 1
            &rfds,          // read fds
            NULL,           // write fds
            NULL,           // except fds
            &tv             // timeout (1 sec)
        );

        if (ret == -1) {
            perror("select error");
            return -1;
        }

        // check scan timeout
        if (get_current_time() - start_time > scan_time) {
            // BLE scan is done
            return counter;
        }

        if (ret == 0) {
            printf("select timeout");
            continue;
        }

        // ret > 0
        unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 };
        int buff_size = read(ble->dd, buf, sizeof(buf));
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
        ba2str(&(adv_info->bdaddr), info.addr);
        ret = eir_parse_name(adv_info->data, adv_info->length, info.name, sizeof(info.name) - 1);
        if (ret == -1) {
            // no name
            continue;
        }

        // check duplicated
        ret = ble_find_index_by_address(*ble_info_list, counter, info.addr);
        if (ret >= 0 || counter >= ble_info_list_len) {
            continue;
        }

        // add info to list
        memcpy(*ble_info_list + counter, &info, sizeof(BLEInfo));
        counter++;
    }
}

// 4. ble_connect
int ble_connect(BLEDevice *ble, const char * address) {

    // 1. get bdaddr
    bdaddr_t bdaddr;
	memset(&bdaddr, 0, sizeof(bdaddr_t));
	str2ba(address, &bdaddr);

    // 2. create connection
    uint16_t handle;
	int ret = hci_le_create_conn(
        ble->dd,
        htobs(0x0004),      // uint16_t interval
        htobs(0x0004),      // uint16_t window
        0x00,               // uint8_t initiator_filter (Use peer address)
        LE_PUBLIC_ADDRESS,  // uint8_t peer_bdaddr_type
        bdaddr,
        LE_PUBLIC_ADDRESS,  // uint8_t own_bdaddr_type
        htobs(0x000F),      // uint16_t min_interval
        htobs(0x000F),      // uint16_t max_interval
        htobs(0x0000),      // uint16_t latency
        htobs(0x0C80),      // uint16_t supervision_timeout
        htobs(0x0001),      // uint16_t min_ce_length
        htobs(0x0001),      // uint16_t max_ce_length
        &handle,
        25000
    );
	if (ret < 0) {
		perror("Could not create connection");
        return -1;
	}

    // success
    printf("Connection handle %d\n", handle);


    usleep(10000);
    printf("disconnect\n");
	hci_disconnect(ble->dd, handle, HCI_OE_USER_ENDED_CONNECTION, 10000);
    return 0;
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

        int ret = memcmp(info->addr, address, sizeof(info->addr));
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
