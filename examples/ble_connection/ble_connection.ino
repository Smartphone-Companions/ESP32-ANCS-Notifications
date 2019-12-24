// @author James Hudson bugs.feedback.whatever@gmail.com
// @todo license text and header - please see github project for license

// A simple example program to retrieve notifications from your device, and output them to the Serial console.


// Header for this library, from https://www.github.com/Smartphone-Companions/ESP32NotificationsLib.git
#include "esp32notifications.h"

/////
// Different hardware presets; uncomment the correct device.
// Or feel free to add your own board layout for your specific hardware.
// See the ESP32 pinout to choose a free GPIO pin on your hardware.

// An inexpensive and easy-to build open-source smartwatch platform https://github.com/jhud/hackwatch
#define HARDWARE_HACKWATCH

#ifdef HARDWARE_HACKWATCH
    #define BUTTON_A    25 // left button
    #define BUTTON_B    26 // center button
    #define BUTTON_C    27 // right button
#else
    #error Hardware buttons not supported!
#endif

// Create an interface to the BLE notification functionality
BLENotifications notifications;

// This callback will be called when a Bluetooth LE connection is made or broken.
// You can update the ESP 32's UI or take other action here.
void onBLEStateChanged(BLENotifications::State state) {
  switch(state) {
      case BLENotifications::StateConnected:
          Serial.println("StateConnected - connected to a phone or tablet"); 
          break;

      case BLENotifications::StateDisconnected:
          Serial.println("StateDisconnected - disconnected from a phone or tablet"); 
		  /* We need to startAdvertising on disconnection, otherwise the ESP 32 will now be invisible.
		  IMO it would make sense to put this in the library to happen automatically, but some people in the Espressif forums
		  were requesting that they would like manual control over re-advertising.*/
		  notifications.startAdvertising(); 
          break; 
  }
}


// A notification arrived from the mobile device, ie a social media notification or incoming call.
void onNotificationArrived(const Notification * notification) {

     Serial.print("Got notification: ");   
     Serial.println(notification->title.c_str());
     Serial.println(notification->message.c_str());
     Serial.println(notification->type.c_str());  
     Serial.println(notifications.getNotificationCategoryDescription(notification->category));  
	Serial.println(notification->categoryCount);  
}


void onNotificationRemoved(const Notification * notification) {

     Serial.print("Removed notification: ");   
     Serial.println(notification->title.c_str());
     Serial.println(notification->message.c_str());
     Serial.println(notification->type.c_str());  
}


// Standard Arduino function which is called once when the device first starts up
void setup() {
  // Button configuration. It is usual to have buttons configured as INPUT_PULLUP in the hardware design,
  // but check the details for your specific device 
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

    Serial.begin(115200);
    while(!Serial) {
        delay(10);
    }

    Serial.println("BLENotifications BLE ANCS on ESP32 Example");
    Serial.println("------------------------------------------");    

    // Set up the BLENotification library
    notifications.begin("BLEConnection device name");
    notifications.setConnectionStateChangedCallback(onBLEStateChanged);
    notifications.setNotificationCallback(onNotificationArrived);
    notifications.setRemovedCallback(onNotificationRemoved);
}


void checkButtons() {
  if (digitalRead(BUTTON_A) == LOW) {
    Serial.println("Positive action."); 
      notifications.actionPositive();
  }
  else if (digitalRead(BUTTON_C) == LOW) {
    Serial.println("Negative action."); 
      notifications.actionNegative();
  }
}


// Standard Arduino function that is called in an endless loop after setup
void loop() {	
    if (notifications.getNumPending() == 0) {
        return;
    }

    checkButtons();
}

/*
X- Connect/Disconnect 
X- Set Name of Peripheral
X- Notification added 
X- Notification removed
X- Notification Attributes
X- Perform Notification Actions (positive or negative)
*/