/*
Minimal iPhone ANCS notification example for ESP32.
Steps:
1) Boot ESP32 with pairing button held (GPIO33 to GND).
2) iPhone: Settings → Bluetooth → pair with "beepr-ancs".
3) Accept pairing and "Allow Notifications" prompt.
4) Reboot ESP32 without the button for normal mode.
*/

#include <Arduino.h>
#include "esp32notifications.h"

static const int PAIRING_PIN = 33;
static const char *DEVICE_NAME = "beepr-ancs";

BLENotifications notifications;

static void onBLEStateChanged(BLENotifications::State state)
{
    switch (state)
    {
    case BLENotifications::StateConnected:
        Serial.println("Connected");
        Serial.println("ANCS client starting (subscribing)");
        break;
    case BLENotifications::StateDisconnected:
        Serial.println("Disconnected");
        notifications.startAdvertising();
        Serial.println("Advertising started");
        break;
    }
}

static void onNotificationArrived(const ArduinoNotification *notification, const Notification *rawNotificationData)
{
    (void)rawNotificationData;
    if (notification->uuid == 0 && notification->type.length() == 0 &&
        notification->title.length() == 0 && notification->message.length() == 0)
    {
        return;
    }

    Serial.println("Notification received");
    Serial.printf("App: %s\n", notification->type.length() ? notification->type.c_str() : "(unknown)");
    Serial.printf("Title: %s\n", notification->title.length() ? notification->title.c_str() : "(none)");
    Serial.printf("Message: %s\n", notification->message.length() ? notification->message.c_str() : "(none)");
    if (notification->time != 0)
    {
        Serial.printf("Date: %lu\n", (unsigned long)notification->time);
    }
    else
    {
        Serial.println("Date: (not provided)");
    }
    Serial.printf("Category: %s\n", notifications.getNotificationCategoryDescription(notification->category));
    Serial.printf("CategoryCount: %u\n", notification->categoryCount);
    Serial.printf("UUID: %lu\n", (unsigned long)notification->uuid);
}

static void onNotificationRemoved(const ArduinoNotification *notification, const Notification *rawNotificationData)
{
    (void)rawNotificationData;
    if (notification->uuid == 0 && notification->type.length() == 0 &&
        notification->title.length() == 0 && notification->message.length() == 0)
    {
        return;
    }
    Serial.println("Notification removed");
    Serial.printf("App: %s\n", notification->type.length() ? notification->type.c_str() : "(unknown)");
    Serial.printf("Title: %s\n", notification->title.length() ? notification->title.c_str() : "(none)");
    Serial.printf("Message: %s\n", notification->message.length() ? notification->message.c_str() : "(none)");
}

void setup()
{
    pinMode(PAIRING_PIN, INPUT_PULLUP);
    bool pairingMode = (digitalRead(PAIRING_PIN) == LOW);

    Serial.begin(115200);
    delay(200);

    if (pairingMode)
    {
        Serial.println("PAIRING MODE (GPIO33=LOW)");
    }
    else
    {
        Serial.println("NORMAL MODE (GPIO33=HIGH)");
    }

    notifications.begin(DEVICE_NAME);
    notifications.setConnectionStateChangedCallback(onBLEStateChanged);
    notifications.setNotificationCallback(onNotificationArrived);
    notifications.setRemovedCallback(onNotificationRemoved);

    notifications.startAdvertising();
    Serial.println("Advertising started");
}

void loop()
{
    delay(250);
}
