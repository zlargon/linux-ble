#include "adv.h"

#include <stdio.h>

// bluez
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

// internal function
static int eir_parse_name(uint8_t *eir, size_t eir_len, char *output_name, size_t output_name_len);
static int eir_parse_data(uint8_t *eir, size_t eir_len);    // TODO

// https://www.bluetooth.com/blog/bluetooth-low-energy-it-starts-with-advertising/
// https://www.novelbits.io/bluetooth-low-energy-sniffer-tutorial-advertisements/

const char * adv_get_adv_type_name(uint8_t adv_type) {
    switch (adv_type) {
        case 0x00:  return "ADV_IND";
        case 0x01:  return "ADV_DIRECT_IND";
        case 0x02:  return "ADV_NONCONN_IND";
        case 0x03:  return "SCAN_REQ";
        case 0x04:  return "SCAN_RSP";
        case 0x05:  return "CONNECT_REQ";
        case 0x06:  return "ADV_SCAN_IND";
        default:    return NULL;
    }
}

const char * adv_get_address_type_name(uint8_t addr_type) {
    switch (addr_type) {
        case LE_PUBLIC_ADDRESS: return "LE_PUBLIC_ADDRESS";
        case LE_RANDOM_ADDRESS: return "LE_RANDOM_ADDRESS";
        default:                return NULL;
    }
}

int adv_parse_data(uint8_t * ptr, Adv * adv) {

    /*
     *  | le_advertising_info                        |
     *  | Type   | Address Type | Address | Data len | Data                               | rssi   |
     *  |                                            | len1 | data1 | len2 | data2 | .... |        |
     *  | 1 byte | 1 byte       | 6 bytes | 1 byte   | 0 ~ 31 bytes                       | 1 byte |
     */


    // parse ADV info
    le_advertising_info *info = (le_advertising_info *) ptr;

    // 1. adv_type
    if (adv_get_adv_type_name(info->bdaddr_type) == NULL) {
        return -1;
    }
    adv->adv_type = info->bdaddr_type;

    // 2. addr_type
    if (adv_get_address_type_name(info->bdaddr_type) == NULL) {
        return -1;
    }
    adv->addr_type = info->bdaddr_type;

    // 3. addr
    ba2str(&(info->bdaddr), adv->addr);

    // parse EIR data

    // 4-1. check data length (0 ~ 31)
    if (0 > info->length || info->length > 31) {
        printf("data length = %d\n", info->length);  // 0 ~ 31
        return -1;
    }

    // 4-2. name
    eir_parse_name(info->data, info->length, adv->name, sizeof(adv->name) - 1);
    // TODO: parse all data

    // 5. rssi (TODO: check rssi)
    ptr += LE_ADVERTISING_INFO_SIZE + info->length;  // 9 + data_len
    adv->rssi = ptr[0];
    ptr++;

    // printf("%s (%s)\n", adv->addr, adv_get_address_type_name(adv->addr_type));
    // printf("    adv type: %s\n", adv_get_adv_type_name(adv->adv_type));
    // printf("    device name: %s\n", adv->name);
    // printf("    rssi: %d\n\n", adv->rssi);

    return 0;
}

// eir_parse_name
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

static int eir_parse_data(uint8_t *eir, size_t eir_len) {
    // TODO
    return 0;
}
