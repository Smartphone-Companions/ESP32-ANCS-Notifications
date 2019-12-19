// Based on the ANCS work of https://github.com/S-March

#include "ble_security.h"
#include "ancs_ble_client.h"

#include "BLEAddress.h"
#include "BLEDevice.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#include "esp_log.h"

#include <Arduino.h> // Only for development

static char LOG_TAG[] = "ANCSBLEClient";

// Fixed service IDs for the Apple ANCS service
const BLEUUID notificationSourceCharacteristicUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
const BLEUUID controlPointCharacteristicUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
const BLEUUID dataSourceCharacteristicUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");
const BLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");


#define MESSAGE_TITLE 1
#define MESSAGE_BODY 3

static ANCSBLEClient * sharedInstance;

static void dataSourceNotifyCallback(
  BLERemoteCharacteristic* pDataSourceCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    ESP_LOGD(LOG_TAG, "Notify callback for characteristic ");
    ESP_LOGD(LOG_TAG, "%s", pDataSourceCharacteristic->getUUID().toString().c_str());
    ESP_LOGD(LOG_TAG, " of data length %d", length);

    for(int i = 0; i < length; i++){
        if(i > 7){
            ESP_LOGD(LOG_TAG, "%02x. ", pData[i]);
        }
        else{
            ESP_LOGD(LOG_TAG, "%02x ", pData[i]);
        }
    }
	
	if ((pData[5] == MESSAGE_BODY || pData[5] == MESSAGE_TITLE) && sharedInstance->notificationCB) {
		pData[length]=0;
		ESP_LOGD(LOG_TAG, "Doing callback");
		sharedInstance->notificationCB(CategoryIDBusinessAndFinance, (char*)pData+8); // @todo data persistance
	}
}


static void notificationSourceNotifyCallback(
  BLERemoteCharacteristic* pNotificationSourceCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify)
{
	ESP_LOGD(LOG_TAG, "notificationSourceNotifyCallback");
    if(pData[0]==0)
    {
      
        ESP_LOGD(LOG_TAG, "New notification!");
        //Serial.println(pNotificationSourceCharacteristic->getUUID().toString().c_str());
        sharedInstance->latestMessageID[0] = pData[4];
        sharedInstance->latestMessageID[1] = pData[5];
        sharedInstance->latestMessageID[2] = pData[6];
        sharedInstance->latestMessageID[3] = pData[7];
        
        ESP_LOGD(LOG_TAG, "Notification type ID (see ANCS docs or higher level functions): %d", (pData[2]));
    }
    sharedInstance->pendingNotification = true;
}

ANCSBLEClient::ANCSBLEClient()
	 : notificationCB(nullptr) {
	assert(sharedInstance == nullptr);
	sharedInstance = this;  
}

void ANCSBLEClient::startClientTask(void * params) {
	ESP_LOGD(LOG_TAG, "Starting client");
		const BLEAddress* address = (BLEAddress*)params;
        BLEClient*  pClient  = BLEDevice::createClient();
        BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
        BLEDevice::setSecurityCallbacks(new NotificationSecurityCallbacks()); // @todo memory leak?

        BLESecurity *pSecurity = new BLESecurity();
        pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
        pSecurity->setCapability(ESP_IO_CAP_IO);
        pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
        // Connect to the remove BLE Server.
        pClient->connect(*address);

        /** BEGIN ANCS SERVICE **/
        // Obtain a reference to the service we are after in the remote BLE server.
        BLERemoteService* pAncsService = pClient->getService(ancsServiceUUID);
        if (pAncsService == nullptr) {
            ESP_LOGD(LOG_TAG, "Failed to find service UUID ancsServiceUUID.");
            return;
        }
        // Obtain a reference to the characteristic in the service of the remote BLE server.
        BLERemoteCharacteristic* pNotificationSourceCharacteristic = pAncsService->getCharacteristic(notificationSourceCharacteristicUUID);
        if (pNotificationSourceCharacteristic == nullptr) {
            ESP_LOGD(LOG_TAG, "Failed to find characteristic UUID notificationSourceCharacteristicUUID");
            return;
        }        
        // Obtain a reference to the characteristic in the service of the remote BLE server.
        sharedInstance->pControlPointCharacteristic = pAncsService->getCharacteristic(controlPointCharacteristicUUID);
        if (sharedInstance->pControlPointCharacteristic == nullptr) {
            ESP_LOGD(LOG_TAG, "Failed to find characteristic UUID: controlPointCharacteristicUUID");
            return;
        }        
        // Obtain a reference to the characteristic in the service of the remote BLE server.
        BLERemoteCharacteristic* pDataSourceCharacteristic = pAncsService->getCharacteristic(dataSourceCharacteristicUUID);
        if (pDataSourceCharacteristic == nullptr) {
            ESP_LOGD(LOG_TAG, "Failed to find characteristic UUID dataSourceCharacteristicUUID");
            return;
        }        
        const uint8_t v[]={0x1,0x0};
        pDataSourceCharacteristic->registerForNotify(dataSourceNotifyCallback);
        pDataSourceCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)v,2,true);
        pNotificationSourceCharacteristic->registerForNotify(notificationSourceNotifyCallback);
        pNotificationSourceCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)v,2,true);

	    while (1)
	    {
	      delay(1000);
		  sharedInstance->update();
	    }
}


BLEUUID ANCSBLEClient::getAncsServiceUUID() {
	return ancsServiceUUID;
}


void ANCSBLEClient::setNotificationArrivedCallback(ble_notification_arrived_t cbNotification) {
	notificationCB = cbNotification;
}


void ANCSBLEClient::update() {
    if(pendingNotification == true){
        // CommandID: CommandIDGetNotificationAttributes
        // 32bit uid
        // AttributeID
        ESP_LOGD(LOG_TAG, "Requesting details...");
        const uint8_t vIdentifier[]={0x0,   latestMessageID[0],latestMessageID[1],latestMessageID[2],latestMessageID[3],   0x0};
        pControlPointCharacteristic->writeValue((uint8_t*)vIdentifier,6,true);
        const uint8_t vTitle[]={0x0,   latestMessageID[0],latestMessageID[1],latestMessageID[2],latestMessageID[3],   0x1, 0x0, 0x10};
        pControlPointCharacteristic->writeValue((uint8_t*)vTitle,8,true);
        const uint8_t vMessage[]={0x0,   latestMessageID[0],latestMessageID[1],latestMessageID[2],latestMessageID[3],   0x3, 0x0, 0x10};
        pControlPointCharacteristic->writeValue((uint8_t*)vMessage,8,true);
        const uint8_t vDate[]={0x0,   latestMessageID[0],latestMessageID[1],latestMessageID[2],latestMessageID[3],   0x5};
        pControlPointCharacteristic->writeValue((uint8_t*)vDate,6,true);
        pendingNotification = false;
    }
    delay(100); //does not work without small delay
}