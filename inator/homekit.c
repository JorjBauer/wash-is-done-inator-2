#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void my_accessory_identify(homekit_value_t _value) {
}

homekit_characteristic_t currentAmbientLightLevel = HOMEKIT_CHARACTERISTIC_(CURRENT_AMBIENT_LIGHT_LEVEL, 0,.min_value = (float[]) {0},);


homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]){
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "Light Sensor"),
                        HOMEKIT_CHARACTERISTIC(MANUFACTURER, "StudioPieters®"),
                        HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "Q39QDPS8GRX8"),
                        HOMEKIT_CHARACTERISTIC(MODEL, "HKVF2T/L"),
                        HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.0.1"),
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                        NULL
                }),
                HOMEKIT_SERVICE(LIGHT_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "Light Sensor"),
                        &currentAmbientLightLevel,
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
