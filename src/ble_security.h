#ifndef BLE_SECURITY_H_
#define BLE_SECURITY_H_

#include "BLESecurity.h"

class NotificationSecurityCallbacks : public BLESecurityCallbacks {

    uint32_t onPassKeyRequest();

    void onPassKeyNotify(uint32_t pass_key);

    bool onSecurityRequest();
    
    bool onConfirmPIN(unsigned int);

    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl);
};

#endif // BLE_SECURITY_H_