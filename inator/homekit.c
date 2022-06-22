#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void my_accessory_identify(homekit_value_t _value) {
}

homekit_characteristic_t leakDetected = HOMEKIT_CHARACTERISTIC_(LEAK_DETECTED,
                                                                0);

homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]){
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "WashIsDoneInator"),
                        HOMEKIT_CHARACTERISTIC(MANUFACTURER, "JorjLabs"),
                        HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "Q39QDPS8GRX8"),
                        HOMEKIT_CHARACTERISTIC(MODEL, "HKVF2T/L"),
                        HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.0.1"),
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                        NULL
                }),
                HOMEKIT_SERVICE(LEAK_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "WashIsDoneInator"),
                        &leakDetected,
                        NULL
                }),
                NULL
        }),
        NULL
};

homekit_server_config_t config = {
        .accessories = accessories,
        .password = "595-33-595",
        .setupId="RW5X",
};
