// Example: Print iOS notifications over Serial using ESP32-ANCS-Notifications.

#include "esp32notifications.h"

BLENotifications notifications;

void onBLEStateChanged(BLENotifications::State state)
{
    switch (state)
    {
    case BLENotifications::StateConnected:
        Serial.println("Connected to iPhone");
        break;
    case BLENotifications::StateDisconnected:
        Serial.println("Disconnected from iPhone");
        notifications.startAdvertising();
        break;
    }
}

void onNotificationArrived(const ArduinoNotification *notification, const Notification *rawNotificationData)
{
    (void)rawNotificationData;
    Serial.println("--- Notification ---");
    Serial.print("App: ");
    Serial.println(notification->type);
    Serial.print("Category: ");
    Serial.println(notifications.getNotificationCategoryDescription(notification->category));
    Serial.print("Title: ");
    Serial.println(notification->title);
    Serial.print("Message: ");
    Serial.println(notification->message);
    Serial.print("Count: ");
    Serial.println(notification->categoryCount);
}

void onNotificationRemoved(const ArduinoNotification *notification, const Notification *rawNotificationData)
{
    (void)rawNotificationData;
    Serial.print("Removed: ");
    Serial.println(notification->title);
}

void setup()
{
    Serial.begin(115200);
    while (!Serial)
    {
        delay(10);
    }

    Serial.println("ESP32 iOS Notifications (ANCS) Example");
    notifications.begin("ESP32-ANCS");
    notifications.setConnectionStateChangedCallback(onBLEStateChanged);
    notifications.setNotificationCallback(onNotificationArrived);
    notifications.setRemovedCallback(onNotificationRemoved);
}

void loop()
{
    delay(1000);
}
