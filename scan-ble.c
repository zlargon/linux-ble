#include <stdio.h>
#include <stdlib.h>         // exit
#include <signal.h>         // signal
#include <sys/select.h>     // select, FD_SET, FD_ZERO
#include <unistd.h>         // read

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

void exit_handler(int param) {
    printf("exit\n");
    // TODO: reset hci_filter
    exit(1);
}

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

int main() {
    // TODO: handles ctrl-c or other orderly exits
    signal(SIGINT, exit_handler);

    int ret = 0;

    // 1. create BLE device
    int dev_id = hci_get_route(NULL);
    int dd = hci_open_dev(dev_id);   // device description
    if (dev_id < 0 || dd < 0) {
        perror("opening socket error");
        return -1;
    }

    printf("dev_id = %d\n", dev_id);
    printf("device_description = %d\n", dd);

    // device info
    struct hci_dev_info dev_info = { 0 };
    ret = hci_devinfo(dev_id, &dev_info);
    if (ret != 0) {
        perror("hci_devinfo failed");
        return -1;
    }
    printf("name = %s\n", dev_info.name);

    // bluetooth device address
    char addr[18] = {0};
    ba2str(&(dev_info.bdaddr), addr);
    printf("addr = %s\n", addr);

    // local_name
    char local_name[HCI_MAX_NAME_LENGTH] = { 0 };
    ret = hci_read_local_name(dd, HCI_MAX_NAME_LENGTH, local_name, 0);
    if (ret != 0) {
        perror("hci_read_local_name failed");
        return -1;
    }
    printf("local_name = %s\n\n", local_name);

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
    if (ret != 0) {
        perror("hci_le_set_scan_enable");
        goto end;
    }

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
        le_advertising_info *info = (le_advertising_info *) (meta->data + 1);

        // show address
        char addr[18] = {0};
        ba2str(&(info->bdaddr), addr);

        // name
		char name[30] = {0};
		ret = eir_parse_name(info->data, info->length, name, sizeof(name) - 1);
        if (ret == 0) {
            printf("%s - %s\n", addr, name);
            // TODO: filter repeated addr
        }
    }

    ret = 0;
end:
    setsockopt(dd, SOL_HCI, HCI_FILTER, &original_filter, original_filter_len);
    return ret;
}
