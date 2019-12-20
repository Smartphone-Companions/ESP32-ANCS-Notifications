// Based on the ANCS work of https://github.com/S-March and the CarWatch project

#include "ble_security.h"
#include "ancs_ble_client.h"
#include "ancs_notification_queue.h"

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


static ANCSBLEClient * sharedInstance;

static void dataSourceNotifyCallback(
  BLERemoteCharacteristic* pDataSourceCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
	 ESP_LOGD(LOG_TAG, "dataSourceNotifyCallback");
	sharedInstance->onDataSourceNotify(pDataSourceCharacteristic, pData, length, isNotify);
}


static void notificationSourceNotifyCallback(
  BLERemoteCharacteristic* pNotificationSourceCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify)
{
	ESP_LOGD(LOG_TAG, "notificationSourceNotifyCallback");
	
	sharedInstance->onNotificationSourceNotify(pNotificationSourceCharacteristic, pData, length, isNotify);
}


ANCSBLEClient::ANCSBLEClient()
	 : notificationCB(nullptr) {
	assert(sharedInstance == nullptr);
	sharedInstance = this;  
	  notificationQueue = new ANCSNotificationQueue();
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
		BLERemoteCharacteristic* pControlPointCharacteristic;
        pControlPointCharacteristic = pAncsService->getCharacteristic(controlPointCharacteristicUUID);
        if (pControlPointCharacteristic == nullptr) {
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
	        uint32_t pendingNotificationId = sharedInstance->notificationQueue->getNextPendingNotification();
	        if (pendingNotificationId != 0)
	        {
	          ESP_LOGD(LOG_TAG, "retriveNotificationData: %d", pendingNotificationId);
	          sharedInstance->retrieveExtraNotificationData(pControlPointCharacteristic, pendingNotificationId);
	        }
	        delay(500);
	    }
}


BLEUUID ANCSBLEClient::getAncsServiceUUID() {
	return ancsServiceUUID;
}


void ANCSBLEClient::setNotificationArrivedCallback(ble_notification_arrived_t cbNotification) {
	notificationCB = cbNotification;
}


void ANCSBLEClient::retrieveExtraNotificationData(BLERemoteCharacteristic * _controlPointCharacteristic, uint32_t notifyUUID) {
	  uint8_t uuid[4];
	  uuid[0] = notifyUUID;
	  uuid[1] = notifyUUID >> 8;
	  uuid[2] = notifyUUID >> 16;
	  uuid[3] = notifyUUID >> 24;
	  const uint8_t vIdentifier[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDAppIdentifier};
	  _controlPointCharacteristic->writeValue((uint8_t *)vIdentifier, 6, true);
	  const uint8_t vTitle[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDTitle, 0x0, 0x10};
	  _controlPointCharacteristic->writeValue((uint8_t *)vTitle, 8, true);
	  const uint8_t vMessage[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDMessage, 0x0, 0x10};
	  _controlPointCharacteristic->writeValue((uint8_t *)vMessage, 8, true);
	  const uint8_t vDate[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDDate};
	  _controlPointCharacteristic->writeValue((uint8_t *)vDate, 6, true);
}

void ANCSBLEClient::onDataSourceNotify(
  BLERemoteCharacteristic* pNotificationSourceCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
	
    std::string message;
	
    uint32_t messageId = pData[4];
    messageId = messageId << 8 | pData[3];
    messageId = messageId << 16 | pData[2];
    messageId = messageId << 24 | pData[1];
    bool notificationAlreadyInQueue = notificationQueue->contains(messageId);
    for (int i = 8; i < length; i++)
    {
      message += (char)pData[i];
    }

	ESP_LOGD(LOG_TAG, "ID: %d raw message: %s isset==%s type==%d", messageId, message.c_str(), notificationAlreadyInQueue ? "true":"false", pData[5]);

    if (notificationAlreadyInQueue == false)
    {
        Notification notification;
        notificationQueue->addNotification(messageId, notification, isIncomingCall(&notification));
    }

	Notification * notification = notificationQueue->getNotification(messageId);

      switch (pData[5])
      {
        case ANCS::NotificationAttributeIDAppIdentifier:
		    notification->type = message;
			ESP_LOGD(LOG_TAG, "got type: %s", message.c_str());
			break;
        case 0x1:
          notification->title = message;
		  ESP_LOGD(LOG_TAG, "got title: %s", message.c_str());
          break;
        case 0x3:
          notification->message = message;
		  ESP_LOGD(LOG_TAG, "got message: %s", message.c_str());
          break;
      }
      if (!notification->title.empty() && !notification->message.empty()) {
		if (notificationCB && notification->isComplete == false) {
			ESP_LOGI(LOG_TAG, "got a full notification: %s - %s ", notification->title.c_str(), notification->message.c_str());
			notificationCB(notification);
		}
		notification->isComplete = true;
      }
}

bool ANCSBLEClient::isIncomingCall(const Notification * notification) const {
	// @todo detect if it is a FaceTime or call
	return false;
}
	
void ANCSBLEClient::onNotificationSourceNotify(
	  BLERemoteCharacteristic *pNotificationSourceCharacteristic,
	  uint8_t *pData,
	  size_t length,
	  bool isNotify)
	{
	  uint32_t messageId;

	  messageId = pData[7];
	  messageId = messageId << 8 | pData[6];
	  messageId = messageId << 16 | pData[5];
	  messageId = messageId << 24 | pData[4];

	  if (pData[0] == ANCS::EventIDNotificationRemoved)
	  {
	    ESP_LOGI(LOG_TAG, "notification removed: %d", messageId);

	    Notification *notification = notificationQueue->getNotification(messageId);

	    if (isIncomingCall(notification))
	    {
	      notificationQueue->addNotification(messageId, *notification, false);
	      notificationQueue->removeCallNotification();
	    }
	  }
	  if (pData[0] == ANCS::EventIDNotificationAdded)
	  {
	    ESP_LOGI(LOG_TAG, "notification added, type: %d", pData[2]);

	    notificationQueue->addPendingNotification(messageId);
	}
}