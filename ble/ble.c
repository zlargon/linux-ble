#include "ble.h"
#include "adv.h"

#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>       // gettimeofday
#include <stdlib.h>         // exit
#include <sys/select.h>     // select, FD_SET, FD_ZERO
#include <unistd.h>         // read
#include <sys/ioctl.h>
#include <errno.h>          // EIO

// Internal Functions
static long long get_current_time();
static int ble_find_index_by_address(BLEDevice * list, size_t list_len, const char * address);

// 1. hci_init
int hci_init(HCIDevice * hci) {
    // 1. create BLE device
    hci->dev_id = hci_get_route(NULL);
    hci->dd = hci_open_dev(hci->dev_id);          // device description
    if (hci->dev_id < 0 || hci->dd < 0) {
        perror("opening socket error");
        return -1;
    }

    printf("dev_id = %d\n", hci->dev_id);
    printf("dd = %d\n", hci->dd);

    // 2. save original filter
    hci_filter_clear(&hci->of);
    socklen_t of_len = sizeof(hci->of);
    int ret = getsockopt(hci->dd, SOL_HCI, HCI_FILTER, &hci->of, &of_len);
    if (ret < 0) {
        perror("getsockopt failed");
        return -1;
    }

    // 3. set new filter
    struct hci_filter nf;
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);       // HCI_EVENT_PKT
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);   // EVT_LE_META_EVENT
    ret = setsockopt(hci->dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));
    if (ret < 0) {
        perror("setsockopt failed");
        return -1;
    }

    return 0;
}

// 2. hci_close
int hci_close(HCIDevice * hci) {
    setsockopt(hci->dd, SOL_HCI, HCI_FILTER, &(hci->of), sizeof(hci->of));
    hci_close_dev(hci->dd);
    memset(hci, 0, sizeof(HCIDevice));
    return 0;
}

// 3. hci_scan_ble
int hci_scan_ble(HCIDevice * hci, BLEDevice * ble_list, int ble_list_len, int scan_time) {
    const long long start_time = get_current_time();

    // scan parameters
    const uint8_t scan_type = 0x01;             // passive: 0x00, active: 0x01
    const uint16_t interval = htobs(0x0010);    // ?
    const uint16_t window   = htobs(0x0010);    // ?
    const uint8_t own_type = LE_PUBLIC_ADDRESS; // LE_PUBLIC_ADDRESS: 0x00, LE_RANDOM_ADDRESS: 0x01
    const uint8_t filter_policy = 0x00;         // no filter
    const int timeout = 10000;

    // 1. set scan params
    int ret = hci_le_set_scan_parameters(hci->dd, scan_type, interval, window, own_type, filter_policy, timeout);
    if (ret != 0) {
        if (errno != EIO) {
            perror("hci_le_set_scan_parameters");
            return -1;
        } else {
            puts("disable bluetooth scan before setting scan params");

            // disable scan before set params
            ret = hci_le_set_scan_enable(
                hci->dd,
                0x00,           // disable
                filter_policy,  // no filter
                timeout
            );
            if (ret != 0) {
                perror("hci_le_set_scan_enable");
                return -1;
            }

            // set scan param again
            ret = hci_le_set_scan_parameters(hci->dd, scan_type, interval, window, own_type, filter_policy, timeout);
            if (ret != 0) {
                // failed again
                perror("hci_le_set_scan_parameters");
                return -1;
            }
        }
    }

    // 2. start scanning
    ret = hci_le_set_scan_enable(
        hci->dd,
        0x01,           // enable
        filter_policy,  // no filter
        timeout
    );
    if (ret != 0) {
        perror("hci_le_set_scan_enable");
        return -1;
    }

    // 3. use 'select' to recieve data from fd
    int counter = 0;
    for (;;) {
        // set read file descriptions
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(hci->dd, &rfds);

        struct timeval tv = {
            .tv_sec  = 1,
            .tv_usec = 0,
        };

        // select read fds
        ret = select(
            hci->dd + 1,    // MAX fd + 1
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
        uint8_t buf[HCI_MAX_EVENT_SIZE] = {};
        const int buff_size = read(hci->dd, buf, sizeof(buf));  // TODO: check buff_size
        if (buff_size == 0) {
            printf("read no data");
            continue;
        }

        /*
         *
         * | HCI_TYPE | HCI_DATA                                          |
         *            | EVENT_TYPE | EVENT_LEN | EVENT_DATA               |
         *                                     | META_TYPE | META_DATA    |
         * | 1 byte   | 1 byte     | 1 byte    | 1 byte    | 0 ~ 32 bytes |
         */

        uint8_t *ptr = buf;

        // 1. HCI Header (1 byte)
        const uint8_t hci_type = ptr[0];    // => EVENT Type
        if (hci_type != HCI_EVENT_PKT) {
            printf("The HCI type (0x%0x2) is not HCI_EVENT_PKT (0x%02x)\n", hci_type, HCI_EVENT_PKT);
            continue;
        }
        ptr += HCI_TYPE_LEN;    // 1 byte

        // 2. EVENT Header (2 bytes)
        const uint8_t event_type = ptr[0];  // => META Type
        const uint8_t event_len  = ptr[1];  // TODO: check event length
        if (event_type != EVT_LE_META_EVENT) {
            printf("The EVENT type (0x%0x2) is not EVT_LE_META_EVENT (0x%02x)\n", event_type, EVT_LE_META_EVENT);
            continue;
        }
        ptr += HCI_EVENT_HDR_SIZE;  // 2 bytes

        // 3. META Header (1 byte)
        const uint8_t meta_type = ptr[0];   // => ADV Type
        if (meta_type != EVT_LE_ADVERTISING_REPORT) {
            printf("META type (0x%02x) is not EVT_LE_ADVERTISING_REPORT (0x%02x)\n", meta_type, EVT_LE_ADVERTISING_REPORT);
            continue;
        }
        ptr += EVT_LE_META_EVENT_SIZE;  // 1 byte

        // 4. ADV Report Numbers (1 byte)
        const uint8_t report_nums = ptr[0];
        if (report_nums > 1) {
            // usually get 1
            printf("[WARN] report_nums = %d. Mutiple Advertising Reports is not handled.\n", report_nums);
        }
        ptr++;

        // 5. Only Parse Frist Advertising Report
        Adv adv = {};
        adv_parse_data(ptr, &adv);

        // 6-1. filter no name
        if (strlen(adv.name) == 0) {
            continue;
        }

        // debug log
        printf("| %s | %s | %s | %d | %s\n",
            adv_get_adv_type_name(adv.adv_type),
            adv_get_address_type_name(adv.addr_type),
            adv.addr,
            adv.rssi,
            adv.name
        );

        // 6-2. filter duplicated address
        ret = ble_find_index_by_address(ble_list, counter, adv.addr);
        if (ret >= 0 || counter >= ble_list_len) {
            // already exist in ble_list
            continue;
        }

        // 7. copy values to ble_list
        BLEDevice * ble = ble_list + counter;
        memcpy(ble->name, adv.name, sizeof(adv.name));  // name
        memcpy(ble->addr, adv.addr, sizeof(adv.addr));  // address
        ble->rssi = adv.rssi;                           // rssi
        ble->hci  = hci;                                // hci

        // 8. increase counter
        counter++;
    }
}

// 4. hci_connect_ble
int hci_connect_ble(HCIDevice * hci, const char * ble_addr) {

    // 1. get bdaddr
    bdaddr_t bdaddr;
	memset(&bdaddr, 0, sizeof(bdaddr_t));
	str2ba(ble_addr, &bdaddr);

    // 2. create connection
    uint16_t handle;
	int ret = hci_le_create_conn(
        hci->dd,
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

    sleep(10);
    printf("disconnect\n");
	hci_disconnect(hci->dd, handle, HCI_OE_USER_ENDED_CONNECTION, 10000);
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
static int ble_find_index_by_address(BLEDevice * list, size_t list_len, const char * address) {
    for (int i = 0; i < list_len; i++) {
        BLEDevice * ble = list + i;

        int ret = memcmp(ble->addr, address, sizeof(ble->addr));
        if (ret == 0) {
            return i;
        }
    }
    return -1;  // not found
}
