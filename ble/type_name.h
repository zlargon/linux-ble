#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>

const char * baseband_name(uint8_t baseband) {
    // hci.h
    switch (baseband) {
        case SCO_LINK:  return "SCO";
        case ACL_LINK:  return "ACL";
        case ESCO_LINK: return "eSCO";
        case 0x80:      return "LE";    // hcitool.c
        default:        return NULL;
    }
}

const char * link_mode_name(uint32_t link_mode) {
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

const char * conn_state_name(uint16_t state) {
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
