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
			StateConnected, 	/// A device connected to the ESP32
			StateDisconnected 	/// A device disconnected from the ESP32
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
		 * @param name the device name, as it will appear in a BLE scan.
		 */
        bool begin(const char * name);
		
        void setConnectionStateChangedCallback(ble_notifications_state_changed_t);
        void setNotificationCallback(ble_notification_arrived_t);

        void actionPositive();
        void actionNegative();
		
		int getNumPending() const { return 0; }
	
		/** 
		 * Given a category, return a description of the category in English
		*/
		const char * getNotificationCategoryDescription(NotificationCategory category) const;
	
	private:
		void startAdvertising();	
		
	private:
		ble_notifications_state_changed_t cbStateChanged;
		ble_notification_arrived_t cbNotification;
		
		class BLEServer* server;
		
		friend class MyServerCallbacks; //Allow internal handlers to access the callbacks of the main class
};

#endif // ESP32NOTIFICATIONS_H_
