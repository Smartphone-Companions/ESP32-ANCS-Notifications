// Based on the original Smartphone Companions ANCS library. GPL-3.0.
#include "ancs_ble_client.h"
#include "NimBLEDevice.h"
#include "NimBLEClient.h"
#include "esp_log.h"
#include <cstring>
#include <Arduino.h>

static const char *TAG = "ANCS";
ANCSBLEClient *ANCSBLEClient::instance = nullptr;
static const NimBLEUUID serviceUUID("7905F431-B5CE-4E99-A40F-4B1E122D00D0");
static const NimBLEUUID sourceUUID("9FBF120D-6301-42D9-8C58-25E699A21DBD");
static const NimBLEUUID controlUUID("69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9");
static const NimBLEUUID dataUUID("22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB");

static void dataNotify(NimBLERemoteCharacteristic *, uint8_t *data, size_t size, bool) {
    if (ANCSBLEClient::instance) ANCSBLEClient::instance->receive(true, data, size);
}
static void sourceNotify(NimBLERemoteCharacteristic *, uint8_t *data, size_t size, bool) {
    if (ANCSBLEClient::instance) ANCSBLEClient::instance->receive(false, data, size);
}

ANCSBLEClient::ANCSBLEClient() {
    packets = xQueueCreate(24, sizeof(Packet));
    events = xQueueCreate(128, sizeof(SmallPacket));
    finished = xSemaphoreCreateBinary();
}
ANCSBLEClient::~ANCSBLEClient() {
    // Owner must stop the worker and deinitialize BLE before destroying us.
    if (packets) vQueueDelete(packets);
    if (events) vQueueDelete(events);
    if (finished) vSemaphoreDelete(finished);
    if (instance == this) instance = nullptr;
}
NimBLEUUID ANCSBLEClient::getAncsServiceUUID() { return serviceUUID; }

bool ANCSBLEClient::start() {
    if (instance || !packets || !events || !finished) return false;
    instance = this;
    stopping.store(false);
    // No core affinity: valid on single-core C3 and dual-core ESP32 alike.
    if (xTaskCreate(taskEntry, "ANCS", 8192, this, 2, &task) != pdPASS) {
        instance = nullptr;
        return false;
    }
    return true;
}
bool ANCSBLEClient::stop() {
    if (!task) return true;
    if (xTaskGetCurrentTaskHandle() == task) return false;
    stopping.store(true);
    linked.store(false);
    // Do not force-delete a task while it owns BLE stack locks.
    if (xSemaphoreTake(finished, pdMS_TO_TICKS(15000)) != pdTRUE) return false;
    task = nullptr;
    return true;
}
void ANCSBLEClient::connected(uint16_t handle) {
    connectionHandle.store(handle);
    authentication.store(0);
    generation.fetch_add(1);
    linked.store(true);
}
void ANCSBLEClient::disconnected() {
    linked.store(false);
    authentication.store(0);
}
void ANCSBLEClient::authenticated(bool success) { authentication.store(success ? 1 : -1); }
bool ANCSBLEClient::active(uint32_t g) const {
    return !stopping.load() && linked.load() && generation.load() == g;
}
void ANCSBLEClient::receive(bool dataSource, const uint8_t *data, size_t length) {
    if (!linked.load() || stopping.load()) return;
    if (!data || length == 0 || length > sizeof(Packet::data)) {
        overflow.store(true);
        return;
    }
    if (!dataSource) {
        if (length != 8) return;
        SmallPacket event{};
        event.generation = generation.load();
        event.kind = Source;
        event.length = length;
        memcpy(event.data, data, length);
        if (xQueueSend(events, &event, 0) != pdTRUE) overflow.store(true);
        return;
    }
    Packet packet{};
    packet.generation = generation.load();
    packet.kind = Data;
    packet.length = length;
    memcpy(packet.data, data, length);
    if (xQueueSend(packets, &packet, 0) != pdTRUE) overflow.store(true);
}
void ANCSBLEClient::performAction(uint32_t uid, uint8_t action) {
    if (!linked.load() || stopping.load() || action > 1) return;
    SmallPacket packet{};
    packet.generation = generation.load();
    packet.kind = Action;
    packet.length = 6;
    packet.data[0] = ANCS::CommandIDPerformNotificationAction;
    for (unsigned i = 0; i < 4; ++i) packet.data[i + 1] = uid >> (8 * i);
    packet.data[5] = action;
    if (xQueueSend(events, &packet, 0) != pdTRUE) ESP_LOGW(TAG, "Action queue full");
}
void ANCSBLEClient::taskEntry(void *self) {
    auto *client = static_cast<ANCSBLEClient *>(self);
    client->run();
    xSemaphoreGive(client->finished);
    vTaskDelete(nullptr);
}
void ANCSBLEClient::reset() {
    ready = false; waiting = false; cancelled = false;
    control = nullptr;
    pending.clear(); cache.clear();
    response.begin(0);
}
void ANCSBLEClient::fail(const char *reason) {
    if (!active(session)) return; // An old session must not disconnect a new peer.
    ESP_LOGW(TAG, "%s; reconnect to retry", reason);
    ready = false;
    linked.store(false);
    NimBLEDevice::getServer()->disconnect(connectionHandle.load());
}
bool ANCSBLEClient::setup(uint32_t g) {
    if (!active(g)) return false;
    // iPhone is the GAP central; ESP32 is the peripheral AND a GATT client.
    // Attach to the existing link. Never call connect() back to the iPhone.
    bleClient = NimBLEDevice::getServer()->getClient(connectionHandle.load());
    if (!bleClient || !active(g)) return false;
    if (bleClient->getConnInfo().isEncrypted()) authentication.store(1);
    else if (!NimBLEDevice::startSecurity(connectionHandle.load())) return false;
    const uint32_t started = millis();
    while (active(g) && authentication.load() == 0 && millis() - started < 30000) delay(20);
    if (!active(g) || authentication.load() != 1) {
        ESP_LOGW(TAG, "Pairing failed/timed out. Accept the iPhone pairing prompt.");
        return false;
    }
    // ANCS may appear only after pairing/authorization. Rediscover on every session.
    NimBLERemoteService *service = nullptr;
    for (unsigned attempt = 0; active(g) && attempt < 5; ++attempt) {
        bleClient->getServices(true);
        service = bleClient->getService(serviceUUID);
        if (service) break;
        delay(1000);
    }
    if (!service || !active(g)) return false;
    auto *source = service->getCharacteristic(sourceUUID);
    auto *data = service->getCharacteristic(dataUUID);
    control = service->getCharacteristic(controlUUID);
    if (!source || !data || !control || !source->canNotify() || !data->canNotify() || !control->canWrite()) return false;
    // Data Source first, then Notification Source; subscribe verifies the CCCD write.
    if (!data->subscribe(true, dataNotify, true) || !active(g)) return false;
    if (!source->subscribe(true, sourceNotify, true) || !active(g)) return false;
    ESP_LOGI(TAG, "ANCS subscribed; waiting for iPhone notifications");
    return true;
}
void ANCSBLEClient::run() {
    while (!stopping.load()) {
        const uint32_t g = generation.load();
        if (linked.load() && g != session) {
            reset();
            session = g;
            overflow.store(false);
            ready = setup(g);
            if (!ready && active(g)) fail("ANCS setup failed; enable Share System Notifications on iPhone");
        }
        if (!active(session)) reset();
        if (overflow.exchange(false) && active(session)) fail("BLE receive queue overflow");
        Packet packet{};
        SmallPacket event{};
        const bool gotEvent = xQueueReceive(events, &event, 0) == pdTRUE;
        if (gotEvent && ready && active(session) && event.generation == session) {
            packet.generation = event.generation;
            packet.kind = event.kind;
            packet.length = event.length;
            memcpy(packet.data, event.data, event.length);
            process(packet);
        }
        if (xQueueReceive(packets, &packet, gotEvent ? 0 : pdMS_TO_TICKS(20)) == pdTRUE &&
            ready && active(session) && packet.generation == session) process(packet);
        if (ready && active(session)) {
            if (waiting && millis() - requestedAt > 10000) {
                // A late fragment has no UID header. Reconnect instead of misattributing it.
                fail("ANCS attribute response timed out");
            } else if (!waiting && uxQueueMessagesWaiting(packets) == 0 && uxQueueMessagesWaiting(events) == 0) requestNext();
        }
    }
    if (bleClient && bleClient->isConnected()) bleClient->disconnect();
    reset();
    // NimBLEServer owns the attached client; leave it alive until BLE deinit.
}
void ANCSBLEClient::requestNext() {
    if (pending.empty() || !control) return;
    current = pending.front(); pending.pop_front();
    response.begin(current.uuid);
    waiting = true; cancelled = false; requestedAt = millis();
    uint8_t request[] = {0, 0, 0, 0, 0, 0, 1, 0x00, 0x01, 3, 0x00, 0x04};
    for (unsigned i = 0; i < 4; ++i) request[i + 1] = current.uuid >> (8 * i);
    // One command, three attributes; at most one response in flight.
    if (!control->writeValue(request, sizeof(request), true)) fail("ANCS attribute request rejected");
}
void ANCSBLEClient::process(const Packet &packet) {
    const uint8_t *p = packet.data;
    if (packet.kind == Action) {
        if (control) {
            uint8_t command[6]; memcpy(command, p, sizeof(command));
            if (!control->writeValue(command, sizeof(command), true)) ESP_LOGW(TAG, "Notification action rejected");
        }
        return;
    }
    if (packet.kind == Data) {
        if (!waiting) return;
        const auto result = response.feed(p, packet.length);
        if (result == ANCSProtocol::Response::Invalid) { fail("Malformed ANCS response"); return; }
        if (result != ANCSProtocol::Response::Complete) return;
        waiting = false;
        if (cancelled) return;
        current.type = response.app; current.title = response.title; current.message = response.message;
        current.isComplete = true;
        if (cache.size() >= 32 && cache.find(current.uuid) == cache.end()) cache.erase(cache.begin());
        cache[current.uuid] = current;
        const auto cb = notificationCB.load();
        if (cb) { const ArduinoNotification n(current); cb(&n, &current); }
        return;
    }
    if (packet.length != 8 || p[0] > ANCS::EventIDNotificationRemoved) return;
    const uint32_t uid = ANCSProtocol::readLE32(p + 4);
    Notification notification;
    notification.uuid = uid; notification.eventFlags = p[1];
    notification.category = static_cast<NotificationCategory>(p[2]); notification.categoryCount = p[3];
    // Coalesce queued modifications; retain FIFO order for other notifications.
    for (auto it = pending.begin(); it != pending.end();) {
        if (it->uuid == uid) it = pending.erase(it); else ++it;
    }
    if (p[0] == ANCS::EventIDNotificationRemoved) {
        if (waiting && current.uuid == uid) cancelled = true;
        auto found = cache.find(uid);
        if (found != cache.end()) { notification = found->second; cache.erase(found); }
        const auto cb = removedCB.load();
        if (cb) { const ArduinoNotification n(notification); cb(&n, &notification); }
    } else {
        if (waiting && current.uuid == uid) cancelled = true;
        if (pending.size() >= 128) { fail("Too many pending notifications"); return; }
        pending.push_back(notification);
    }
}
