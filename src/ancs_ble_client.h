#ifndef ANCS_BLE_CLIENT_H_
#define ANCS_BLE_CLIENT_H_

#include "ble_notification.h"

#include "FreeRTOS.h" // For asynchronous tasks

class BLEAddress;
class BLERemoteCharacteristic;


#include "BLEUUID.h"

/**
 * Internal module for creating and managing an ANCS client connection singleton.
 * This needs to run asynchronously as a FreeRTOS task.
 */
class ANCSBLEClient {
public:
   /**
    * Become a BLE client to a remote BLE server. Pass in address of the server.
    */
	ANCSBLEClient();
	void setNotificationArrivedCallback(ble_notification_arrived_t cbNotification);

public:
	static BLEUUID getAncsServiceUUID(); // To be able to advertise it
	static void startClientTask(void *data);
	
public:
	uint8_t latestMessageID[4]; // needs to be public so the CBs can access it
	bool pendingNotification = false;
	ble_notification_arrived_t notificationCB;
	xTaskHandle clientTaskHandle;
	void update();
	
private:
	BLERemoteCharacteristic* pControlPointCharacteristic;
};

#endif // ANCS_BLE_CLIENT_H_