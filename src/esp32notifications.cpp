
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

// NKolban's original library has a setServiceSolicitation method. As of writing, this is not in the
// Espressif core libs. If you are using NKolban's branch of the library, or if this moves into
// the core libraries eventually, uncomment this.
// #define BLE_LIB_HAS_SERVICE_SOLICITATION

#include "esp32notifications.h"
#include "ancs_ble_client.h"
#include "ble_security.h"
#include "ble_compat.h"

#include "BLEAddress.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLE2902.h"

#include <cstring>

#include <esp_gatts_api.h>

static char LOG_TAG[] = "BLENotifications";

extern const BLEUUID ancsServiceUUID;

#ifndef BLE_LIB_HAS_SERVICE_SOLICITATION
// Use a static function, instead of doing a whole private implementation just for a this one small patch.
static void setServiceSolicitation(class BLEAdvertisementData &advertisementData, BLEUUID uuid);
static void addAdvertisementDataBinary(class BLEAdvertisementData &advertisementData, const uint8_t *data, size_t len);
#endif

class MyServerCallbacks : public BLEServerCallbacks
{

public:
	BLENotifications *instance;

	MyServerCallbacks(BLENotifications *parent)
			: instance(parent)
	{
	}

	void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
	{
		ESP_LOGI(LOG_TAG, "Device connected");
		instance->client = new ANCSBLEClient(); // @todo memory leaks?
		instance->client->setNotificationArrivedCallback(instance->cbNotification);
		instance->client->setNotificationRemovedCallback(instance->cbRemoved);
		::xTaskCreatePinnedToCore(&ANCSBLEClient::startClientTask, "ClientTask", 10000, new BLEAddress(param->connect.remote_bda), 5, &instance->client->clientTaskHandle, 0);

		delay(1000);

		ESP_LOGI(LOG_TAG, "Set up client");

		if (instance->cbStateChanged)
		{
			instance->cbStateChanged(BLENotifications::StateConnected);
		}
	};

	void onDisconnect(BLEServer *pServer)
	{
		::vTaskDelete(instance->client->clientTaskHandle);
		instance->client->clientTaskHandle = nullptr;
		ESP_LOGI(LOG_TAG, "Device disconnected");
		if (instance->cbStateChanged)
		{
			instance->cbStateChanged(BLENotifications::StateDisconnected);
		}
		delete instance->client;
		instance->client = nullptr;
	}
};

BLENotifications::BLENotifications()
		: cbStateChanged(nullptr), client(nullptr), isAdvertising(false)
{
}

const char *BLENotifications::getNotificationCategoryDescription(NotificationCategory category) const
{
	switch (category)
	{
	case CategoryIDOther:
		return "other";
	case CategoryIDIncomingCall:
		return "incoming call";
	case CategoryIDMissedCall:
		return "missed call";
	case CategoryIDVoicemail:
		return "voicemail";
	case CategoryIDSocial:
		return "social";
	case CategoryIDSchedule:
		return "schedule";
	case CategoryIDEmail:
		return "email";
	case CategoryIDNews:
		return "news";
	case CategoryIDHealthAndFitness:
		return "health and fitness";
	case CategoryIDBusinessAndFinance:
		return "business and finance";
	case CategoryIDLocation:
		return "location";
	case CategoryIDEntertainment:
		return "entertainment";
	default:
		return "unknown";
	}
}

bool BLENotifications::begin(const char *name)
{
	ESP_LOGI(LOG_TAG, "begin()");
	BLEDevice::init(name);
	deviceName = String(name);
	server = BLEDevice::createServer();
	server->setCallbacks(new MyServerCallbacks(this));
	ble_compat::installSecurityCallbacks(); // @todo memory leak?

	startAdvertising();
	return true;
}

bool BLENotifications::stop()
{
	ESP_LOGI(LOG_TAG, "stop()");
	BLEDevice::deinit(false);
	return true;
}

void BLENotifications::setConnectionStateChangedCallback(ble_notifications_state_changed_t callback)
{
	cbStateChanged = callback;
}

void BLENotifications::setNotificationCallback(ble_notification_arrived_t callback)
{
	cbNotification = callback;
}

void BLENotifications::setRemovedCallback(ble_notification_removed_t callback)
{
	cbRemoved = callback;
}

void BLENotifications::actionPositive(uint32_t uuid)
{
	ESP_LOGI(LOG_TAG, "actionPositive()");
	client->performAction(uuid, uint8_t(ANCS::NotificationActionPositive));
}

void BLENotifications::actionNegative(uint32_t uuid)
{
	ESP_LOGI(LOG_TAG, "actionNegative()");
	client->performAction(uuid, uint8_t(ANCS::NotificationActionNegative));
}

void BLENotifications::startAdvertising()
{
	ESP_LOGI(LOG_TAG, "startAdvertising()");

	// Start soliciting the Apple ANCS service and make the device visible to searches on iOS (from Apple ANCS documentation)
	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

	if (isAdvertising == true)
	{
		// Stopping it without checking first seems to cause failures to advertise without debugging on.
		// There is no way to query the BLEAdvertising object to see if it is advertising, so we keep a variable.
		pAdvertising->stop();
	}

	BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
#if defined(ESP_BLE_ADV_FLAG_GEN_DISC) && defined(ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
	oAdvertisementData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
#else
	oAdvertisementData.setFlags(0x06);
#endif
	if (deviceName.length() > 0)
	{
		oAdvertisementData.setName(deviceName);
	}

#ifdef BLE_LIB_HAS_SERVICE_SOLICITATION
	oAdvertisementData.setServiceSolicitation(ANCSBLEClient::getAncsServiceUUID());
#else
	setServiceSolicitation(oAdvertisementData, ANCSBLEClient::getAncsServiceUUID());
#endif

	pAdvertising->setAdvertisementData(oAdvertisementData);

	if (deviceName.length() > 0)
	{
		BLEAdvertisementData scanResponseData;
		scanResponseData.setName(deviceName);
		pAdvertising->setScanResponse(true);
		pAdvertising->setScanResponseData(scanResponseData);
	}

	// Set security
	BLESecurity *pSecurity = new BLESecurity();
	ble_compat::configureSecurityServer(pSecurity);

	// Start advertising
	pAdvertising->start();
	isAdvertising = true;

	ESP_LOGD(LOG_TAG, "Advertising started!");
}

#ifndef BLE_LIB_HAS_SERVICE_SOLICITATION
void setServiceSolicitation(BLEAdvertisementData &advertisementData, BLEUUID uuid)
{
	uint8_t data[18];
	switch (uuid.bitSize())
	{
	case 16:
	{
		// [Len] [0x14] [UUID16] data
		data[0] = 3;
		data[1] = ESP_BLE_AD_TYPE_SOL_SRV_UUID; // 0x14
		memcpy(&data[2], &uuid.getNative()->uuid.uuid16, 2);
		addAdvertisementDataBinary(advertisementData, data, 4);
		break;
	}

	case 128:
	{
		// [Len] [0x15] [UUID128] data
		data[0] = 17;
		data[1] = ESP_BLE_AD_TYPE_128SOL_SRV_UUID; // 0x15
		memcpy(&data[2], uuid.getNative()->uuid.uuid128, 16);
		addAdvertisementDataBinary(advertisementData, data, 18);
		break;
	}

	default:
		return;
	}
}

void addAdvertisementDataBinary(BLEAdvertisementData &advertisementData, const uint8_t *data, size_t len)
{
	advertisementData.addData((char *)data, len);
}
#endif
