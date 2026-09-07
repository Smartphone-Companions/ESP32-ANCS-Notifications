#include "esp32notifications.h"
#include "ancs_ble_client.h"
#include "NimBLEDevice.h"
#include "NimBLEServer.h"
#include "NimBLEAdvertising.h"
#include "esp_log.h"

static const char *TAG = "ANCS";

class MyServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit MyServerCallbacks(BLENotifications *parent) : instance(parent) {}
    void onConnect(NimBLEServer *server, NimBLEConnInfo &info) override {
        if (server->getConnectedCount() > 1) {
            server->disconnect(info.getConnHandle());
            return;
        }
        instance->isAdvertising = false;
        handle = info.getConnHandle();
        instance->client->connected(handle);
        auto cb = instance->cbStateChanged.load();
        if (cb) cb(BLENotifications::StateConnected);
    }
    void onDisconnect(NimBLEServer *, NimBLEConnInfo &info, int) override {
        if (info.getConnHandle() != handle) return;
        handle = 0xFFFF;
        if (instance->client) instance->client->disconnected();
        auto cb = instance->cbStateChanged.load();
        if (cb && !instance->stopped.load()) cb(BLENotifications::StateDisconnected);
    }
    void onAuthenticationComplete(NimBLEConnInfo &info) override {
        if (info.getConnHandle() != handle || !instance->client) return;
        instance->client->authenticated(info.isEncrypted());
        if (info.isEncrypted()) ESP_LOGI(TAG, "BLE link encrypted");
        else ESP_LOGW(TAG, "Pairing failed; forget device on iPhone and retry");
    }
private:
    uint16_t handle = 0xFFFF;
    BLENotifications *instance;
};

BLENotifications::BLENotifications() = default;

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

bool BLENotifications::begin(const char *name) {
    if (begun || stopped.load() || ANCSBLEClient::instance || !name) return false;
    deviceName = std::string(name).substr(0, 29);
    if (!NimBLEDevice::init(name)) return false;
    // Headless C3: encrypted, bonded Just Works pairing; no claimed display/keypad.
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    client = new ANCSBLEClient();
    client->setNotificationArrivedCallback(cbNotification.load());
    client->setNotificationRemovedCallback(cbRemoved.load());
    if (!client->start()) {
        delete client; client = nullptr;
        NimBLEDevice::deinit(true);
        return false;
    }
    server = NimBLEDevice::createServer();
    serverCallbacks = new MyServerCallbacks(this);
    server->setCallbacks(serverCallbacks, false);
    begun = true;
    startAdvertising();
    if (!isAdvertising.load()) { stop(); return false; }
    return true;
}

bool BLENotifications::stop() {
    if (!begun) return true;
    // Calling stop() from an application notification callback would join itself.
    // The worker detects this and leaves BLE intact.
    stopped.store(true);
    if (!client->stop()) {
        ESP_LOGW(TAG, "Worker still running; call stop() again from loop()");
        return false;
    }
    if (!NimBLEDevice::deinit(false)) return false;
    NimBLEDevice::deinit(true); // Host is stopped; now release owned objects.
    delete client; client = nullptr;
    delete serverCallbacks; serverCallbacks = nullptr;
    server = nullptr; begun = false; isAdvertising = false;
    return true;
}
void BLENotifications::setConnectionStateChangedCallback(ble_notifications_state_changed_t cb) {
    cbStateChanged.store(cb);
}
void BLENotifications::setNotificationCallback(ble_notification_arrived_t cb) {
    cbNotification.store(cb);
    if (client) client->setNotificationArrivedCallback(cb);
}
void BLENotifications::setRemovedCallback(ble_notification_removed_t cb) {
    cbRemoved.store(cb);
    if (client) client->setNotificationRemovedCallback(cb);
}
void BLENotifications::actionPositive(uint32_t uid) {
    if (client) client->performAction(uid, ANCS::NotificationActionPositive);
}
void BLENotifications::actionNegative(uint32_t uid) {
    if (client) client->performAction(uid, ANCS::NotificationActionNegative);
}
void BLENotifications::startAdvertising() {
    if (!begun || stopped.load()) return;
    auto *advertising = NimBLEDevice::getAdvertising();
    if (isAdvertising.exchange(false)) advertising->stop();
    NimBLEAdvertisementData data;
    data.setFlags(0x06); // General discoverable, BR/EDR unsupported.
    // ANCS 128-bit service solicitation in Bluetooth little-endian wire order.
    const uint8_t solicitation[] = {0x11, 0x15, 0xD0, 0x00, 0x2D, 0x12,
        0x1E, 0x4B, 0x0F, 0xA4, 0x99, 0x4E, 0xCE, 0xB5,
        0x31, 0xF4, 0x05, 0x79};
    data.addData(solicitation, sizeof(solicitation));
    advertising->setAdvertisementData(data);
    // Scan response keeps the name separate from the 31-byte primary payload.
    NimBLEAdvertisementData scan;
    scan.setName(deviceName);
    advertising->setScanResponseData(scan);
    advertising->enableScanResponse(true);
    if (!advertising->start()) {
        ESP_LOGE(TAG, "Advertising failed");
        return;
    }
    isAdvertising.store(true);
}
