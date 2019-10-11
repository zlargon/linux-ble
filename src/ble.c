#include "ble.h"
#include "nameof.h"

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
static int ble_find_index_by_address(BLEDevice * list, size_t list_len, bdaddr_t * addr);
static int eir_parse_name(uint8_t *eir, size_t eir_len, char *output_name, size_t output_name_len);

// 1. hci_init
int hci_init(HCIDevice * hci) {
    // 1. create BLE device
    hci->dev_id = hci_get_route(NULL);
    hci->dd = hci_open_dev(hci->dev_id);          // device description
    if (hci->dev_id < 0 || hci->dd < 0) {
        perror("opening socket error");
        return -1;
    }

    // 2. get HCI bluetooth device address
    struct hci_dev_info dev_info = {};
    int ret = hci_devinfo(hci->dev_id, &dev_info);
    if (ret != 0) {
        perror("hci_devinfo failed");
        return -1;
    }

    // 3. get HCI name
    ret = hci_read_local_name(hci->dd, HCI_MAX_NAME_LENGTH, hci->name, 0);
    if (ret != 0) {
        perror("hci_read_local_name failed");
        return -1;
    }

    // 4. set address
    memcpy(&(hci->addr), &(dev_info.bdaddr), sizeof(bdaddr_t)); // addr
    ba2str(&(hci->addr), hci->addr_s);                          // addr_s

    printf("| id = %d | sock = %d | %s | %s\n",
        hci->dev_id,
        hci->dd,
        hci->addr_s,
        hci->name
    );

    // 5. save original filter
    hci_filter_clear(&hci->of);
    socklen_t of_len = sizeof(hci->of);
    ret = getsockopt(hci->dd, SOL_HCI, HCI_FILTER, &hci->of, &of_len);
    if (ret < 0) {
        perror("getsockopt failed");
        return -1;
    }

    return 0;
}

// 2. hci_close
int hci_close(HCIDevice * hci) {
    hci_close_dev(hci->dd);
    memset(hci, 0, sizeof(HCIDevice));
    return 0;
}

// 3. hci_update_conn_list
int hci_update_conn_list(HCIDevice * hci) {
    // reset hci_conn
    hci->conn.dev_id = hci->dev_id;
    hci->conn.num    = HCI_MAX_CONN;

    // create socket
    int sock = socket(AF_BLUETOOTH, SOCK_RAW | SOCK_CLOEXEC, BTPROTO_HCI);
    if (sock < 0) {
        perror("create socket failed");
        return -1;
    }

    // HCI Get Conn List
    int ret = ioctl(sock, HCIGETCONNLIST, &(hci->conn));
    if (ret == -1) {
        perror("ioctl HCIGETCONNLIST failed");
    }

    close(sock);
    return ret;
}

// 4. hci_scan_ble
int hci_scan_ble(HCIDevice * hci, BLEDevice * ble_list, int ble_list_len, int scan_time) {
    const long long start_time = get_current_time();

    // 1. set new filter
    struct hci_filter nf = {};
    hci_filter_clear(&nf);
    hci_filter_set_ptype(HCI_EVENT_PKT, &nf);       // HCI_EVENT_PKT
    hci_filter_set_event(EVT_LE_META_EVENT, &nf);   // EVT_LE_META_EVENT
    int ret = setsockopt(hci->dd, SOL_HCI, HCI_FILTER, &nf, sizeof(nf));
    if (ret < 0) {
        perror("setsockopt failed");
        return -1;
    }

    // scan parameters
    const uint8_t scan_type = 0x01;             // passive: 0x00, active: 0x01
    const uint16_t interval = htobs(0x0010);    // ?
    const uint16_t window   = htobs(0x0010);    // ?
    const uint8_t own_type = LE_PUBLIC_ADDRESS; // LE_PUBLIC_ADDRESS: 0x00, LE_RANDOM_ADDRESS: 0x01
    const uint8_t filter_policy = 0x00;         // no filter
    const int timeout = 10000;

    // 2. set scan params
    ret = hci_le_set_scan_parameters(hci->dd, scan_type, interval, window, own_type, filter_policy, timeout);
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

    // 3. start scanning
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

    // 4. use 'select' to recieve data from fd
    int counter = 0;
    for (;;) {
        // check scan time
        if (get_current_time() - start_time > scan_time) {
            // BLE scan is done
            break;
        }

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

        // check select result
        if (ret ==  0) continue;    // select timeout => select again
        if (ret == -1) {
            perror("select error");
            counter = -1;
            break;
        }

        // ret > 0
        uint8_t buf[HCI_MAX_EVENT_SIZE] = {};
        const int buff_size = read(hci->dd, buf, sizeof(buf));
        if (buff_size < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }

            counter = -1;
            break;
        }

        if (buff_size == 0) {
            // end of file
            break;
        }

        /*
         *
         * | HCI_TYPE | HCI_DATA                                          |
         *            | EVENT_TYPE | EVENT_LEN | EVENT_DATA               |
         *                                     | META_TYPE | META_DATA    |
         * | 1 byte   | 1 byte     | 1 byte    | 1 byte    | 0 ~ 32 bytes |
         */
        struct _pkt {
            // HCI
            uint8_t hci_type;   // type
                                // data
            // event
            uint8_t evt_type;   // type
            uint8_t evt_len;    // len
                                // data
            // meta
            uint8_t meta_type;  // type
                                // data

            /*
             * Advertisement: (0 ~ 32 bytes)
             * https://stackoverflow.com/questions/26275679/
             *
             * | adv_info_num | adv_info_1 | adv_info_2 | adv_info_3 | .... |
             * | 1 byte       | 0 ~ 31 bytes                                |
             */

            // adverisement report number (usually be 1)
            uint8_t adv_info_num;

            // only handle first adv info
            /*
             *  Advertisement Info
             *
             *  | Type   | Address Type | Address | Data len | Data                                 | rssi   |
             *  |                                            | type1 | data1 | type2 | data2 | .... |        |
             *  | 1 byte | 1 byte       | 6 bytes | 1 byte   | n bytes                              | 1 byte |
             */
            struct {
                uint8_t  type;       // advertising type
                uint8_t  addr_type;  // address type
                bdaddr_t addr;       // address (6 bytes)
                uint8_t  data_len;   // data length
                uint8_t  data[0];    // data start pointer
            } adv_info[1];

        } * pkt = (struct _pkt *) buf;

        if (pkt->hci_type != HCI_EVENT_PKT) {
            printf("The HCI type (0x%0x2) is not HCI_EVENT_PKT (0x%02x)\n", pkt->hci_type, HCI_EVENT_PKT);
            continue;
        }

        if (pkt->evt_type != EVT_LE_META_EVENT) {
            printf("The EVENT type (0x%0x2) is not EVT_LE_META_EVENT (0x%02x)\n", pkt->evt_type, EVT_LE_META_EVENT);
            continue;
        }

        if (pkt->meta_type != EVT_LE_ADVERTISING_REPORT) {
            printf("The META type (0x%02x) is not EVT_LE_ADVERTISING_REPORT (0x%02x)\n", pkt->meta_type, EVT_LE_ADVERTISING_REPORT);
            continue;
        }

        if (pkt->adv_info_num > 1) {
            // usually get 1
            printf("[WARN] adv_info_num = %d. Mutiple Advertising Reports is not handled.\n", pkt->adv_info_num);
        }

        // 1. name (filter empty name)
        char name[30] = {};
        eir_parse_name(pkt->adv_info[0].data, pkt->adv_info[0].data_len, name, sizeof(name) - 1);
        if (strlen(name) == 0) {
            continue;
        }

        // 2. rssi
        int8_t rssi = *(pkt->adv_info[0].data + pkt->adv_info[0].data_len);

        // 3. addr_s
        char addr_s[18] = {};
        ba2str(&(pkt->adv_info[0].addr), addr_s);

        // debug log
        printf("| %8s | %s | %s | %d | %s\n",
            nameof_adv_type(pkt->adv_info[0].type),
            nameof_bdaddr_type(pkt->adv_info[0].addr_type),
            addr_s,
            rssi,
            name
        );

        // 4. decide to update original device, or add new one to ble_list
        BLEDevice * ble;
        int idx = ble_find_index_by_address(ble_list, counter, &pkt->adv_info[0].addr);
        if (idx >= 0) {
            ble = ble_list + idx;           // already exist in ble_list => update the old one
        } else if (counter < ble_list_len) {
            ble = ble_list + counter;       // add new device to ble_list
            counter++;                      // increase counter
        } else {
            printf("[warn] ble_list (%d) is full to add new devices\n", ble_list_len);
            continue;
        }

        // 5. copy values to ble instance
        memcpy(ble->name, name, sizeof(name));                        // name
        ble->rssi = rssi;                                             // rssi
        ble->addr_type = pkt->adv_info[0].addr_type;                  // addr_type
        memcpy(&ble->addr, &pkt->adv_info[0].addr, sizeof(bdaddr_t)); // addr
        memcpy(ble->addr_s, addr_s, sizeof(addr_s));                  // addr_s
        ble->hci = hci;                                               // hci
    }

    // 5. stop scanning
    ret = hci_le_set_scan_enable(
        hci->dd,
        0x00,           // disable
        filter_policy,  // no filter
        timeout
    );
    if (ret != 0) {
        perror("hci_le_set_scan_enable");
    }

    // 6. reset filter
    ret = setsockopt(hci->dd, SOL_HCI, HCI_FILTER, &(hci->of), sizeof(hci->of));
    if (ret != 0) {
        perror("setsockopt faild");
    }

    return counter;
}

// 5. ble_connect
int ble_connect(BLEDevice * ble) {

    HCIDevice * hci = ble->hci;

    // 1. get bdaddr
    bdaddr_t bdaddr = {};
    memset(&bdaddr, 0, sizeof(bdaddr_t));
    str2ba(ble->addr_s, &bdaddr);

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
    ble->handle = handle;
    return 0;
}

// 6. ble_disconnect
int ble_disconnect(BLEDevice * ble) {
    int ret = hci_disconnect(
        ble->hci->dd,
        ble->handle,
        HCI_OE_USER_ENDED_CONNECTION,
        10000
    );

    if (ret == 0) {
        ble->handle = 0;
    } else {
        perror("hci_disconnect failed");
    }

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
static int ble_find_index_by_address(BLEDevice * list, size_t list_len, bdaddr_t * addr) {
    for (int i = 0; i < list_len; i++) {
        BLEDevice * ble = list + i;
        if (bacmp(&ble->addr, addr) == 0) {
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
