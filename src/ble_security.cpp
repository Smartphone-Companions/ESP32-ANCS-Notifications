
#include "ble_security.h"

#include "esp_log.h"

static char LOG_TAG[] = "NotificationSecurityCallbacks";



uint32_t NotificationSecurityCallbacks::onPassKeyRequest(){
    ESP_LOGI(LOG_TAG, "PassKeyRequest");
    return 123456;
}

void NotificationSecurityCallbacks::onPassKeyNotify(uint32_t pass_key){
    ESP_LOGI(LOG_TAG, "On passkey Notify number:%d", pass_key);
}

bool NotificationSecurityCallbacks::onSecurityRequest(){
    ESP_LOGI(LOG_TAG, "On Security Request");
    return true;
}

bool NotificationSecurityCallbacks::onConfirmPIN(unsigned int){
    ESP_LOGI(LOG_TAG, "On Confirmed Pin Request");
    return true;
}

void NotificationSecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl){
    ESP_LOGI(LOG_TAG, "Starting BLE work!");
    if(cmpl.success){
        uint16_t length;
        esp_ble_gap_get_whitelist_size(&length);
        ESP_LOGD(LOG_TAG, "size: %d", length);
    }
}
