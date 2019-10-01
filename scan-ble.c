#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>
#include <unistd.h>     // read

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

#define TIMEOUT 1000

#define CHECK_RET                                                   \
    if (ret != 0) {                                                 \
        printf("%d: %s (%d)\n", __LINE__, strerror(errno), errno);  \
        return -1;                                                  \
    }                                                               \

void exit_handler(int param) {
    printf("exit\n");
    exit(1);
}


int main() {
    int ret;    // tmp return value

    // handles ctrl-c or other orderly exits
    signal(SIGINT, exit_handler);

    // create BLE device
    int dev_id = hci_get_route(NULL);
    int dd = hci_open_dev(dev_id);   // device description
    if (dev_id < 0 || dd < 0) {
        perror("opening socket error");
        return -1;
    }

    printf("Device Id: %d\n", dev_id);
    printf("Device Description: %d\n", dd);


    // struct hci_dev_info {
    //     uint16_t dev_id;
    //     char     name[8];

    //     bdaddr_t bdaddr;

    //     uint32_t flags;
    //     uint8_t  type;

    //     uint8_t  features[8];

    //     uint32_t pkt_type;
    //     uint32_t link_policy;
    //     uint32_t link_mode;

    //     uint16_t acl_mtu;
    //     uint16_t acl_pkts;
    //     uint16_t sco_mtu;
    //     uint16_t sco_pkts;

    //     struct   hci_dev_stats stat;
    // };

    // int hci_devinfo(int dev_id, struct hci_dev_info *di);

    struct hci_dev_info dev_info = { 0 };
    ret = hci_devinfo(dev_id, &dev_info);
    if (ret != 0) {
        perror("hci_devinfo failed");
        return -1;
    }

    printf("dev_id = %d\n", dev_info.dev_id);
    printf("name = %s\n", dev_info.name);

    // show address
    char addr[18] = {0};
    ba2str(&(dev_info.bdaddr), addr);
    printf("addr = %s\n", addr);

    // int hci_read_local_name(int dd, int len, char *name, int to);
    char local_name[HCI_MAX_NAME_LENGTH] = { 0 };
    ret = hci_read_local_name(dd, HCI_MAX_NAME_LENGTH, local_name, 0);
    if (ret != 0) {
        perror("hci_read_local_name failed");
        return -1;
    }
    printf("local_name = %s\n", local_name);

    // int hci_read_remote_name(int dd, const bdaddr_t *bdaddr, int len, char *name, int to)
    // char remote_name[HCI_MAX_NAME_LENGTH] = { 0 };
    // ret = hci_read_remote_name(dd, &(dev_info.bdaddr), sizeof(remote_name), remote_name, 1000);
    // if (ret != 0) {
    //     perror("hci_read_remote_name failed");
    //     return -1;
    // }
    // printf("remote_name = %s\n", remote_name);
    // return 0;

    // save original filter
    struct hci_filter original_filter;
    hci_filter_clear(&original_filter);
    socklen_t original_filter_len = sizeof(original_filter);
    getsockopt(dd, SOL_HCI, HCI_FILTER, &original_filter, &original_filter_len);

    // setup new filter
    struct hci_filter new_filter;
    hci_filter_clear(&new_filter);
    hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);       // HCI_EVENT_PKT
    hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);   // EVT_LE_META_EVENT
    setsockopt(dd, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter));

    // disable scanning just in case scanning was already happening,
    // otherwise hci_le_set_scan_parameters call will fail

    // 1. always disable scan
    ret = hci_le_set_scan_enable(
        dd,
        0x00,       // disable: 0x00, enable: 0x01
        0,          // no filter
        TIMEOUT
    );
    if (ret != 0 && errno != EIO) {
        perror("Disable BLE Scan");
        return -1;
    }


    // 2. set scan params
    ret = hci_le_set_scan_parameters(
        dd,
        0x00,               // type?
        // 0x01,               // type?
        htobs(0x0010),      // interval
        htobs(0x0010),      // window
        0x00,               // own_type
        0x00,               // no filter
        TIMEOUT
    );
    CHECK_RET

    // 3. start scanning
    ret = hci_le_set_scan_enable(
        dd,
        0x01,       // enable
        0,          // no filter
        TIMEOUT
    );
    CHECK_RET

    ///////////////////////////////////////////////////////////////////////////

    printf("\nScanning ...\n");


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
            return -1;
        }

        if (ret == 0) {
            printf("select timeout");
            continue;
        }

        // ret > 0
        unsigned char buf[HCI_MAX_EVENT_SIZE] = { 0 };
        int buff_size = read(dd, buf, sizeof(buf));
        if (buff_size == 0) {
            printf("read: no data");
            continue;
        }
        printf("buff_size = %d\n", buff_size);


        // get body ptr and body size
        unsigned char *ptr = buf + (1 + HCI_EVENT_HDR_SIZE);    // +1 ?
        buff_size -= (1 + HCI_EVENT_HDR_SIZE);                  // TODO: more buffer?

        // EVT_LE_META_EVENT	    0x3E
        // EVT_LE_META_EVENT_SIZE   1
        // evt_le_meta_event {
        //     uint8_t		subevent;
        //     uint8_t		data[0];
        // }

        // parse to meta
        evt_le_meta_event *meta = (evt_le_meta_event *) ptr;
        if (meta->subevent != EVT_LE_ADVERTISING_REPORT) {  // 0x02
            printf("meta->subevent (%d) is not EVT_LE_ADVERTISING_REPORT (%d)\n",
                meta->subevent,
                EVT_LE_ADVERTISING_REPORT
            );
            continue;
        }

        // EVT_LE_ADVERTISING_REPORT	0x02
        // LE_ADVERTISING_INFO_SIZE     9
        // le_advertising_info {
        //     uint8_t		evt_type;
        //     uint8_t		bdaddr_type;
        //     bdaddr_t	    bdaddr;
        //     uint8_t		length;
        //     uint8_t		data[0];
        // }

        // parse ADV info
        le_advertising_info *info = (le_advertising_info *) (meta->data + 1);

        // show address
        char addr[18];
        ba2str(&(info->bdaddr), addr);

        // remote name
        char name[HCI_MAX_NAME_LENGTH] = {0};
        ret = hci_read_remote_name(dd, &(info->bdaddr), sizeof(name), name, 1000);
        // if (ret != 0) {
        //     perror("read remote name failed");
        // } else {
        //     printf("%s - %s\n", addr, name);
        //     break;  // debug
        // }

        printf("%s - %s\n", addr, name);
    }

    return 0;
}
