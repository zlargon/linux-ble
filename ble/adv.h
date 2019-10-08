#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t adv_type;
    uint8_t addr_type;
    char    addr[18];
    int8_t  rssi;

    // EIR Data
    char name[30];
} Adv;

const char * adv_get_adv_type_name(uint8_t adv_type);
const char * adv_get_address_type_name(uint8_t addr_type);
int adv_parse_data(uint8_t * ptr, Adv * adv);
