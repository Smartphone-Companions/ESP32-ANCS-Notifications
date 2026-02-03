// Based on the ANCS work of https://github.com/S-March and the CarWatch project

#include "ble_security.h"
#include "ble_compat.h"
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

static const uint8_t kAttrFlagType = 1 << 0;
static const uint8_t kAttrFlagTitle = 1 << 1;
static const uint8_t kAttrFlagMessage = 1 << 2;

// Fixed service IDs for the Apple ANCS service
const BLEUUID notificationSourceCharacteristicUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
const BLEUUID controlPointCharacteristicUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
const BLEUUID dataSourceCharacteristicUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");
const BLEUUID ancsServiceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");

static ANCSBLEClient *sharedInstance;

static void dataSourceNotifyCallback(
		BLERemoteCharacteristic *pDataSourceCharacteristic,
		uint8_t *pData,
		size_t length,
		bool isNotify)
{
	ESP_LOGD(LOG_TAG, "dataSourceNotifyCallback");
	sharedInstance->onDataSourceNotify(pDataSourceCharacteristic, pData, length, isNotify);
}

static void notificationSourceNotifyCallback(
		BLERemoteCharacteristic *pNotificationSourceCharacteristic,
		uint8_t *pData,
		size_t length,
		bool isNotify)
{
	ESP_LOGD(LOG_TAG, "notificationSourceNotifyCallback");
	sharedInstance->onNotificationSourceNotify(pNotificationSourceCharacteristic, pData, length, isNotify);
}

ANCSBLEClient::ANCSBLEClient()
		: notificationCB(nullptr), removedCB(nullptr), pControlPointCharacteristic(nullptr)
{
	assert(sharedInstance == nullptr);
	sharedInstance = this;
	notificationQueue = new ANCSNotificationQueue();
}

ANCSBLEClient::~ANCSBLEClient()
{
	sharedInstance = nullptr;
}

void ANCSBLEClient::startClientTask(void *params)
{
	ESP_LOGD(LOG_TAG, "Starting client");
	const BLEAddress *address = (BLEAddress *)params;
	sharedInstance->setup(address);

	ANCSNotificationQueue *queue = sharedInstance->notificationQueue;
	while (1)
	{
		if (queue->pendingNotificationExists())
		{
			Notification pending = queue->getNextPendingNotification();
			ESP_LOGD(LOG_TAG, "retriveNotificationData: %d", pending.uuid);
			sharedInstance->retrieveExtraNotificationData(pending);
		}
		delay(500);
	}
}

void ANCSBLEClient::setup(const BLEAddress *address)
{
	BLEClient *pClient = BLEDevice::createClient();
	ble_compat::installSecurityCallbacks(); // @todo memory leak?

	BLESecurity *pSecurity = new BLESecurity();
	ble_compat::configureSecurityClient(pSecurity);
	// Connect to the remote BLE Server.
	if (!pClient->connect(*address))
	{
		ESP_LOGW(LOG_TAG, "Failed to connect to remote BLE server.");
		return;
	}

	const bool secureOk = ble_compat::secureConnectionIfSupported(pClient, 0);
	if (!secureOk)
	{
		ESP_LOGW(LOG_TAG, "secureConnection() failed; ANCS may not work without pairing/bonding.");
	}

	/** BEGIN ANCS SERVICE **/
	// Obtain a reference to the service we are after in the remote BLE server.
	BLERemoteService *pAncsService = pClient->getService(ancsServiceUUID);
	if (pAncsService == nullptr)
	{
		ESP_LOGW(LOG_TAG, "Failed to find service UUID ancsServiceUUID.");
		return;
	}
	// Obtain a reference to the characteristic in the service of the remote BLE server.
	BLERemoteCharacteristic *pNotificationSourceCharacteristic = pAncsService->getCharacteristic(notificationSourceCharacteristicUUID);
	if (pNotificationSourceCharacteristic == nullptr)
	{
		ESP_LOGW(LOG_TAG, "Failed to find characteristic UUID notificationSourceCharacteristicUUID");
		return;
	}
	// Obtain a reference to the characteristic in the service of the remote BLE server.
	pControlPointCharacteristic = pAncsService->getCharacteristic(controlPointCharacteristicUUID);
	if (pControlPointCharacteristic == nullptr)
	{
		ESP_LOGW(LOG_TAG, "Failed to find characteristic UUID: controlPointCharacteristicUUID");
		return;
	}
	// Obtain a reference to the characteristic in the service of the remote BLE server.
	BLERemoteCharacteristic *pDataSourceCharacteristic = pAncsService->getCharacteristic(dataSourceCharacteristicUUID);
	if (pDataSourceCharacteristic == nullptr)
	{
		ESP_LOGW(LOG_TAG, "Failed to find characteristic UUID dataSourceCharacteristicUUID");
		return;
	}

	const uint8_t v[] = {0x1, 0x0};
	pDataSourceCharacteristic->registerForNotify(dataSourceNotifyCallback);
	pDataSourceCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t *)v, 2, true);
	pNotificationSourceCharacteristic->registerForNotify(notificationSourceNotifyCallback);
	pNotificationSourceCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t *)v, 2, true);
}

BLEUUID ANCSBLEClient::getAncsServiceUUID()
{
	return ancsServiceUUID;
}

void ANCSBLEClient::setNotificationArrivedCallback(ble_notification_arrived_t cbNotification)
{
	notificationCB = cbNotification;
}

void ANCSBLEClient::setNotificationRemovedCallback(ble_notification_removed_t cbNotification)
{
	removedCB = cbNotification;
}

void ANCSBLEClient::retrieveExtraNotificationData(Notification &pending)
{
	uint8_t uuid[4];
	uint32_t notifyUUID = pending.uuid;
	uuid[0] = notifyUUID;
	uuid[1] = notifyUUID >> 8;
	uuid[2] = notifyUUID >> 16;
	uuid[3] = notifyUUID >> 24;

	bool notificationAlreadyInQueue = notificationQueue->contains(pending.uuid);
	if (notificationAlreadyInQueue == false)
	{
		notificationQueue->addNotification(notifyUUID, pending, isIncomingCall(pending));
	}

	const uint8_t vIdentifier[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDAppIdentifier};
	pControlPointCharacteristic->writeValue((uint8_t *)vIdentifier, 6, true);
	const uint16_t maxAttrLen = ANCS_ATTRIBUTE_MAX_LEN;
	const uint8_t vTitle[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDTitle,
														uint8_t(maxAttrLen & 0xFF), uint8_t((maxAttrLen >> 8) & 0xFF)};
	pControlPointCharacteristic->writeValue((uint8_t *)vTitle, 8, true);
	const uint8_t vMessage[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDMessage,
															uint8_t(maxAttrLen & 0xFF), uint8_t((maxAttrLen >> 8) & 0xFF)};
	pControlPointCharacteristic->writeValue((uint8_t *)vMessage, 8, true);
	const uint8_t vDate[] = {0x0, uuid[0], uuid[1], uuid[2], uuid[3], ANCS::NotificationAttributeIDDate};
	pControlPointCharacteristic->writeValue((uint8_t *)vDate, 6, true);
}

void ANCSBLEClient::onDataSourceNotify(
		BLERemoteCharacteristic *pNotificationSourceCharacteristic,
		uint8_t *pData,
		size_t length,
		bool isNotify)
{
	if (length < 8)
	{
		ESP_LOGW(LOG_TAG, "Data source notify too short: %d", (int)length);
		return;
	}

	std::string message;

	uint32_t messageId = pData[4];
	messageId = messageId << 8 | pData[3];
	messageId = messageId << 16 | pData[2];
	messageId = messageId << 24 | pData[1];

	const uint8_t attributeId = pData[5];
	const uint16_t attributeLength = uint16_t(pData[6]) | (uint16_t(pData[7]) << 8);
	const size_t payloadLength = length - 8;

	for (size_t i = 8; i < length; i++)
	{
		message += (char)pData[i];
	}

	ESP_LOGD(LOG_TAG, "ID: %d raw message: %s type==%d", messageId, message.c_str(), attributeId);

	Notification *notification = notificationQueue->getNotification(messageId);
	if (notification == nullptr)
	{
		ESP_LOGW(LOG_TAG, "No notification found for id %d", messageId);
		return;
	}

	std::string *target = nullptr;
	uint16_t *expectedLength = nullptr;
	uint8_t attributeFlag = 0;

	switch (attributeId)
	{
	case ANCS::NotificationAttributeIDAppIdentifier:
		target = &notification->type;
		expectedLength = &notification->expectedTypeLength;
		attributeFlag = kAttrFlagType;
		break;
	case ANCS::NotificationAttributeIDTitle:
		target = &notification->title;
		expectedLength = &notification->expectedTitleLength;
		attributeFlag = kAttrFlagTitle;
		break;
	case ANCS::NotificationAttributeIDMessage:
		target = &notification->message;
		expectedLength = &notification->expectedMessageLength;
		attributeFlag = kAttrFlagMessage;
		break;
	default:
		break;
	}

	if (target != nullptr)
	{
		if (*expectedLength == 0)
		{
			*expectedLength = attributeLength;
			if (!target->empty())
			{
				target->clear();
			}
		}

		if (attributeLength == 0)
		{
			notification->attributeFlags |= attributeFlag;
		}
		else if (payloadLength > 0)
		{
			const size_t currentLength = target->size();
			size_t appendLength = payloadLength;
			if (currentLength + appendLength > *expectedLength)
			{
				appendLength = *expectedLength - currentLength;
			}
			target->append(reinterpret_cast<const char *>(pData + 8), appendLength);

			if (target->size() >= *expectedLength)
			{
				notification->attributeFlags |= attributeFlag;
			}
		}
	}

	const bool hasType = (notification->attributeFlags & kAttrFlagType) != 0;
	const bool hasTitle = (notification->attributeFlags & kAttrFlagTitle) != 0;
	const bool hasMessage = (notification->attributeFlags & kAttrFlagMessage) != 0;
	// iOS "Show Previews" off can send empty message/title with a valid app identifier.
	const bool hasAnyMetadata = hasType || hasTitle || hasMessage;
	const bool hasFullPayload = hasType && (hasTitle || hasMessage);

	if (hasAnyMetadata && (!notification->showed || (notification->showed && !notification->isComplete && hasFullPayload)))
	{
		if (notificationCB)
		{
			ESP_LOGI(LOG_TAG, "got a notification: %s - %s ", notification->title.c_str(), notification->message.c_str());
			const ArduinoNotification arduinoNotification = ArduinoNotification(*notification);
			notificationCB(&arduinoNotification, notification);
		}
		notification->showed = true;
	}

	if (hasFullPayload)
	{
		notification->isComplete = true;
	}
}

bool ANCSBLEClient::isIncomingCall(const Notification &notification) const
{
	// @todo detect if it is a FaceTime or call by app? Or is category sufficient?
	return notification.category == CategoryIDIncomingCall;
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

		if (isIncomingCall(*notification))
		{
			notificationQueue->addNotification(messageId, *notification, false);
			notificationQueue->removeCallNotification();
		}

		if (removedCB)
		{
			const ArduinoNotification arduinoNotification = ArduinoNotification(*notification);
			removedCB(&arduinoNotification, notification);
		}
	}
	else if (pData[0] == ANCS::EventIDNotificationAdded)
	{
		ESP_LOGI(LOG_TAG, "notification added, type: %d", pData[2]);
		Notification pending;
		pending.uuid = messageId;
		pending.eventFlags = pData[1];
		pending.category = NotificationCategory(pData[2]);
		pending.categoryCount = pData[3];
		notificationQueue->addPendingNotification(pending);
	}
}

void ANCSBLEClient::performAction(uint32_t notifyUUID, uint8_t actionID)
{
	uint8_t uuid[4];
	uuid[0] = notifyUUID;
	uuid[1] = notifyUUID >> 8;
	uuid[2] = notifyUUID >> 16;
	uuid[3] = notifyUUID >> 24;

	const uint8_t vPerformAction[] = {ANCS::CommandIDPerformNotificationAction, uuid[0], uuid[1], uuid[2], uuid[3], actionID};
	pControlPointCharacteristic->writeValue((uint8_t *)vPerformAction, (sizeof(vPerformAction) / sizeof(vPerformAction[0])), true);
}
