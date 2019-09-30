// https://people.csail.mit.edu/albert/bluez-intro/c404.html#bzi-choosing
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

int main() {

    int dev_id = hci_get_route(NULL);   // device id
    int dd = hci_open_dev(dev_id);      // device description
    if (dev_id < 0 || dd < 0) {
        perror("opening socket");
        return -1;
    }

    printf("Device Id          = %d\n", dev_id);
    printf("Device Description = %d\n\n", dd);


    // inquiry_info {
    //     bdaddr_t	bdaddr;
    //     uint8_t		pscan_rep_mode;
    //     uint8_t		pscan_period_mode;
    //     uint8_t		pscan_mode;
    //     uint8_t		dev_class[3];
    //     uint16_t	clock_offset;
    // }
    inquiry_info *ii = NULL; // must set NULL
    int num_rsp = hci_inquiry(
        dev_id,
        8,                  // int len
        255,                // int nrsp
        NULL,               // const uint8_t *lap
        &ii,
        IREQ_CACHE_FLUSH    // long flags
    );
    if (num_rsp < 0) {
        perror("hci_inquiry");
        return -1;
    }


    // show address and remote_name
    for (int i = 0; i < num_rsp; i++) {
        inquiry_info * info = ii + i;

        // address
        char addr[19] = {0};
        ba2str(&info->bdaddr, addr);

        // remote name
        char name[248] = {0};
        int ret = hci_read_remote_name(dd, &info->bdaddr, sizeof(name), name, 0);
        if (ret != 0) {
            strcpy(name, "[unknown]");
        }

        printf("%s - %s\n", addr, name);
    }

    free(ii);
    hci_close_dev(dd);
    return 0;
}
