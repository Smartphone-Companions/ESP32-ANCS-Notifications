#ifndef ESP32NOTIFICATIONS_H_
#define ESP32NOTIFICATIONS_H_

#include <Arduino.h>

#include "ble_notification.h"

/**
 * Arduino library for the ESP32, for receiving BLE notifications from another device.
 *
 * This class was designed with simplicity and ease-of-use in mind.
 * This library supports ESP32 debugging output: in the Arduino IDE, change Tools/Core Debug Level.
 */
class BLENotifications {
    public:
        /**
         * State of the BLE connection.
         */
        enum State {
            StateConnected,     /**< A device connected to the ESP32. */
            StateDisconnected   /**< A device disconnected from the ESP32. Note that you should call startAdvertising after this message.*/
        };
        
        /**
         * Callback for a state change of the BLE connection.
         * Use this to modify the UI to notify the user if a connection is available.
         */
        typedef void (*ble_notifications_state_changed_t)(State state);


    public:
        BLENotifications();
        
        /**
         * Setup the device to receive BLE notifications.
         * Will also automatically start advertising.
         * @param name the device name, as it will appear in a BLE scan.
         */
        bool begin(const char * name);
        
        /**
         * Make this device visible in scans as available for connection.
         * Will stop the current advertisement if it is running.
         */
        void startAdvertising();
        
        /**
         * Shutdown BLE and free memory.
         * Note that currently the BLE library internals do not seem to restore state properly, so the BLE library
         * cannot be used after a stop.
         * 
         */
        bool stop();
        
        /**
         * Set this to a callback for when the BLE connects or disconnects.
         */
        void setConnectionStateChangedCallback(ble_notifications_state_changed_t);
        
        /**
         * Set this to a callback for when a notification arrives.
         */
        void setNotificationCallback(ble_notification_arrived_t);
        void setRemovedCallback(ble_notification_removed_t);

        void actionPositive(uint32_t uuid);
        void actionNegative(uint32_t uuid);
    
        /** 
         * Given a category, return a description of the category in English
        */
        const char * getNotificationCategoryDescription(NotificationCategory category) const;

	private:

        
    private:
        ble_notifications_state_changed_t cbStateChanged;
        ble_notification_arrived_t cbNotification;
        ble_notification_removed_t cbRemoved;
        
        class BLEServer* server;
        class ANCSBLEClient* client;
        
        bool isAdvertising;
        
        friend class MyServerCallbacks; //Allow internal handlers to access the callbacks of the main class
};

#endif // ESP32NOTIFICATIONS_H_
