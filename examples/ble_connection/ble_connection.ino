#include <esp32notifications.h>
#include <atomic>

BLENotifications notifications;
std::atomic<bool> advertiseAgain{false};

void onState(BLENotifications::State state) {
    if (state == BLENotifications::StateConnected) {
        Serial.println("BLE connected. Accept pairing and notification sharing on your iPhone.");
    } else {
        Serial.println("BLE disconnected.");
        advertiseAgain.store(true); // Restart from loop(), outside the Bluetooth callback.
    }
}
void onNotification(const ArduinoNotification *n, const Notification *) {
    Serial.printf("\nUID: %lu | Category: %s | Flags: 0x%02lx\n",
                  (unsigned long)n->uuid,
                  notifications.getNotificationCategoryDescription(n->category),
                  (unsigned long)n->eventFlags);
    Serial.println("App: " + n->type);
    Serial.println("Title: " + n->title);
    Serial.println("Message: " + n->message);
    // Notification pointers are valid only during this callback.
    // actionPositive(n->uuid) / actionNegative(n->uuid) enqueue an action.
    // Only invoke actions when the corresponding eventFlags bit is present.
}
void onRemoved(const ArduinoNotification *n, const Notification *) {
    Serial.printf("Removed UID: %lu\n", (unsigned long)n->uuid);
}
void setup() {
    Serial.begin(115200);
    const uint32_t started = millis();
    while (!Serial && millis() - started < 3000) delay(10);
    notifications.setConnectionStateChangedCallback(onState);
    notifications.setNotificationCallback(onNotification);
    notifications.setRemovedCallback(onRemoved);
    if (!notifications.begin("ESP32-C3 ANCS")) Serial.println("ANCS initialization failed.");
    else Serial.println("Open iPhone Settings > Bluetooth and pair with ESP32-C3 ANCS.");
}
void loop() {
    if (advertiseAgain.exchange(false)) notifications.startAdvertising();
    delay(10);
}
