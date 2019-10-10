#include "nameof.h"

const char * nameof_baseband(uint8_t baseband) {
    // hci.h
    switch (baseband) {
        case SCO_LINK:  return "SCO";
        case ACL_LINK:  return "ACL";
        case ESCO_LINK: return "eSCO";
        case 0x80:      return "LE";    // hcitool.c
        default:        return NULL;
    }
}

const char * nameof_link_mode(uint32_t link_mode) {
    // hci.h
    switch (link_mode) {
        case 0:               return "NONE";
        case HCI_LM_ACCEPT:   return "ACCEPT";
        case HCI_LM_MASTER:   return "MASTER";
        case HCI_LM_AUTH:     return "AUTH";
        case HCI_LM_ENCRYPT:  return "ENCRYPT";
        case HCI_LM_TRUSTED:  return "TRUSTED";
        case HCI_LM_RELIABLE: return "RELIABLE";
        case HCI_LM_SECURE:   return "SECURE";
        default:              return NULL;
    }
}

const char * nameof_conn_state(uint16_t state) {
    // bluetooth.h
    switch (state) {
        case BT_CONNECTED:  return "CONNECTED";
        case BT_OPEN:       return "OPEN";
        case BT_BOUND:      return "BOUND";
        case BT_LISTEN:     return "LISTEN";
        case BT_CONNECT:    return "CONNECT";
        case BT_CONNECT2:   return "CONNECT2";
        case BT_CONFIG:     return "CONFIG";
        case BT_DISCONN:    return "DISCONN";
        case BT_CLOSED:     return "CLOSED";
        default:            return NULL;
    }
}

const char * nameof_adv_type(uint8_t adv_type) {
    // https://www.bluetooth.com/blog/bluetooth-low-energy-it-starts-with-advertising/
    // https://www.novelbits.io/bluetooth-low-energy-sniffer-tutorial-advertisements/
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

const char * nameof_bdaddr_type(uint8_t bdaddr_type) {
    // hci.h
    switch (bdaddr_type) {
        case LE_PUBLIC_ADDRESS: return "LE_PUBLIC_ADDRESS";
        case LE_RANDOM_ADDRESS: return "LE_RANDOM_ADDRESS";
        default:                return NULL;
    }
}
