#include "esp32notifications.h"

BLENotifications::BLENotifications() {
	
}


bool BLENotifications::begin(const char * name) {
	return false;
}


void BLENotifications::setConnectionStateChangedCallback(ble_notifications_state_changed_t) {
	
}


void BLENotifications::setNotificationCallback(ble_notification_arrived_t) {
	
}


void BLENotifications::actionPositive() {
	
}


void BLENotifications::actionNegative() {

}


