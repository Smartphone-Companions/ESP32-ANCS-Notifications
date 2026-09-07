#pragma once
#include "ble_notification.h"
#include "ancs_protocol.h"
#include "NimBLEUUID.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <atomic>
#include <deque>
#include <map>

class NimBLEClient;
class NimBLERemoteCharacteristic;

// One persistent worker owns GATT operations and all notification state.
// Bluetooth callbacks only copy packets; they never wait for GATT operations.
class ANCSBLEClient {
public:
    ANCSBLEClient();
    ~ANCSBLEClient();
    bool start();
    bool stop();
    void connected(uint16_t handle);
    void disconnected();
    void authenticated(bool success);
    void performAction(uint32_t uid, uint8_t action);
    void receive(bool dataSource, const uint8_t *data, size_t length);
    void setNotificationArrivedCallback(ble_notification_arrived_t cb) { notificationCB.store(cb); }
    void setNotificationRemovedCallback(ble_notification_removed_t cb) { removedCB.store(cb); }
    static NimBLEUUID getAncsServiceUUID();
    static ANCSBLEClient *instance;
private:
    enum Kind : uint8_t { Source, Data, Action };
    struct Packet {
        uint32_t generation;
        uint16_t length;
        Kind kind;
        uint8_t data[512];
    };
    struct SmallPacket {
        uint32_t generation;
        uint16_t length;
        Kind kind;
        uint8_t data[8];
    };
    static void taskEntry(void *self);
    void run();
    bool setup(uint32_t generation);
    bool active(uint32_t generation) const;
    void process(const Packet &packet);
    void requestNext();
    void reset();
    void fail(const char *reason);
    QueueHandle_t packets = nullptr;
    QueueHandle_t events = nullptr;
    SemaphoreHandle_t finished = nullptr;
    TaskHandle_t task = nullptr;
    std::atomic<bool> stopping{false}, linked{false}, overflow{false};
    std::atomic<int> authentication{0};
    std::atomic<uint32_t> generation{0};
    std::atomic<uint16_t> connectionHandle{0xFFFF};
    std::atomic<ble_notification_arrived_t> notificationCB{nullptr};
    std::atomic<ble_notification_removed_t> removedCB{nullptr};
    NimBLEClient *bleClient = nullptr;
    NimBLERemoteCharacteristic *control = nullptr;
    uint32_t session = 0;
    bool ready = false, waiting = false, cancelled = false;
    uint32_t requestedAt = 0;
    Notification current;
    ANCSProtocol::Response response;
    std::deque<Notification> pending;
    std::map<uint32_t, Notification> cache;
};
