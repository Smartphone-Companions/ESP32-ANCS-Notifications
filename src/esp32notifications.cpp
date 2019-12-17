// Based on https://github.com/espressif/esp-idf/tree/master/examples/bluetooth/bluedroid/ble/ble_ancs

#include "esp32notifications.h"
#include "ancs_ble_client.h"
#include "ble_security.h"

#include "BLEAddress.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#include <esp_gatts_api.h>

static char LOG_TAG[] = "BLENotifications";

extern const BLEUUID ancsServiceUUID;

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
        //ANCSBLEClient* pMyClient = new ANCSBLEClient(); // @todo memory leak?
        //pMyClient->setStackSize(18000); // @todo not needed?
		ancs_ble_client_init(new BLEAddress(connectEventParam->remote_bda)); // @todo - memory leak?
		
		delay(1000);
		
		// Grab any pending notifications as a test
		ancs_ble_client_update();
		
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
    BLEDevice::setSecurityCallbacks(new NotificationSecurityCallbacks()); // @todo memory leak?
	
    // Start soliciting the Apple ANCS service and make the device visible to searches on iOS (from Apple ANCS documentation)
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
    oAdvertisementData.setFlags(0x01);
    oAdvertisementData.setServiceSolicitation(getAncsServiceUUID()); 
    pAdvertising->setAdvertisementData(oAdvertisementData);        

    // Set security
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
    pSecurity->setCapability(ESP_IO_CAP_OUT);
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

    //Start advertising
    pAdvertising->start();
}


