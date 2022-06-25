#include <homekit/homekit.h>
#include <homekit/characteristics.h>

void my_accessory_identify(homekit_value_t _value) {
}

// Only the sensor state is used for alerting. The other data shows in details if you
// go looking for it... not useful to me at the moment, so I'm turning it all off.
homekit_characteristic_t sensorState = HOMEKIT_CHARACTERISTIC_(CONTACT_SENSOR_STATE,
                                                               0);
/*
homekit_characteristic_t statusFault = HOMEKIT_CHARACTERISTIC_(STATUS_FAULT,
                                                               0);
homekit_characteristic_t statusActive = HOMEKIT_CHARACTERISTIC_(STATUS_ACTIVE,
                                                                0);
homekit_characteristic_t statusTampered = HOMEKIT_CHARACTERISTIC_(STATUS_TAMPERED,
                                                                  0);
homekit_characteristic_t statusLowBattery = HOMEKIT_CHARACTERISTIC_(STATUS_LOW_BATTERY,
                                                                    0);
*/
homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]){
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "WashIsDoneInator"),
                        HOMEKIT_CHARACTERISTIC(MANUFACTURER, "JorjLabs"),
                        HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "703a0bb7632b32"), // 12 random digits
                        HOMEKIT_CHARACTERISTIC(MODEL, "v2"),
                        HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, my_accessory_identify),
                        NULL
                }),
                HOMEKIT_SERVICE(CONTACT_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
                        HOMEKIT_CHARACTERISTIC(NAME, "WashIsDoneInator"),
                        &sensorState,
                        /*
                        &statusFault,
                        &statusActive,
                        &statusTampered,
                        &statusLowBattery,*/
                        NULL
                }),
                NULL
        }),
        NULL
};

homekit_server_config_t config = {
        .accessories = accessories,
        .password = "177-95-554",
        .setupId="M3E1",
};
