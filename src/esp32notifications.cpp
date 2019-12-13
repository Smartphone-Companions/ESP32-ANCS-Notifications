// Based on https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/ble_ancs

#include "esp32notifications.h"

#include "BLEAddress.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#include <esp_gatts_api.h>

static char LOG_TAG[] = "BLENotifications";

class MySecurity : public BLESecurityCallbacks {

    uint32_t onPassKeyRequest(){
        ESP_LOGI(LOG_TAG, "PassKeyRequest");
        return 123456;
    }

    void onPassKeyNotify(uint32_t pass_key){
        ESP_LOGI(LOG_TAG, "On passkey Notify number:%d", pass_key);
    }

    bool onSecurityRequest(){
        ESP_LOGI(LOG_TAG, "On Security Request");
        return true;
    }
    
    bool onConfirmPIN(unsigned int){
        ESP_LOGI(LOG_TAG, "On Confrimed Pin Request");
        return true;
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl){
        ESP_LOGI(LOG_TAG, "Starting BLE work!");
        if(cmpl.success){
            uint16_t length;
            esp_ble_gap_get_whitelist_size(&length);
            ESP_LOGD(LOG_TAG, "size: %d", length);
        }
    }
};


class MyServerCallbacks: public BLEServerCallbacks {
private:
    struct gatts_connect_evt_param { // @todo include from sdk/include/bt/esp_gatts_api.h
        uint16_t conn_id;               /*!< Connection id */
        esp_bd_addr_t remote_bda;       /*!< Remote bluetooth device address */
    } connect; 
	
public:
	BLENotifications * instance;
	
	MyServerCallbacks(BLENotifications * parent)
	 	: instance(parent) {
	}
	
    void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param) {
		ESP_LOGI(LOG_TAG, "Device connected");
        Serial.println("**Device connected**");
		gatts_connect_evt_param * connectEventParam = (gatts_connect_evt_param *) param;
        Serial.println(BLEAddress(connectEventParam->remote_bda).toString().c_str());
        /*MyClient* pMyClient = new MyClient();
        pMyClient->setStackSize(18000);
        pMyClient->start(new BLEAddress(BLEDevice::m_remoteBda));*/ // @todo - memory leak?
        if (instance->cbStateChanged) {
        	instance->cbStateChanged(BLENotifications::StateConnected);
        }
    };

void onDisconnect(BLEServer* pServer) {
		ESP_LOGI(LOG_TAG, "Device disconnected");
        Serial.println("**Device disconnected**");
        if (instance->cbStateChanged) {
        	instance->cbStateChanged(BLENotifications::StateDisconnected);
        }
		// @todo, disconnect, free stack?
    }
};


BLENotifications::BLENotifications()
	 : cbStateChanged(0)
{
	
}



bool BLENotifications::begin(const char * name) {
    BLEDevice::init(name);
    server = BLEDevice::createServer();
    server->setCallbacks(new MyServerCallbacks(this));
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    BLEDevice::setSecurityCallbacks(new MySecurity());
	
	startAdvertising();
}


void BLENotifications::setConnectionStateChangedCallback(ble_notifications_state_changed_t callback) {
	cbStateChanged = callback;
}


void BLENotifications::setNotificationCallback(ble_notification_arrived_t) {
	
}


void BLENotifications::actionPositive() {
	
}


void BLENotifications::actionNegative() {

}

void BLENotifications::startAdvertising() {
    // Start soliciting ANCS
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    oAdvertisementData.setFlags(0x01);
    oAdvertisementData.setServiceSolicitation(BLEUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0"));
    pAdvertising->setAdvertisementData(oAdvertisementData);        

    // Set security
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setCapability(ESP_IO_CAP_OUT);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    //Start advertising
    pAdvertising->start();
}


