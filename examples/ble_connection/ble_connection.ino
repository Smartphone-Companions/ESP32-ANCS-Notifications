// @author James Hudson bugs.feedback.whatever@gmail.com
// @todo license text and header - please see github project for license

#include "esp32notifications.h"

// Different hardware presets; feel free to add your own board layout.
// You will need to add these hardware buttons to your dev board.
// See the ESP32 pinout to choose a free GPIO pin.
#define HARDWARE_HACKWATCH

#ifdef HARDWARE_HACKWATCH
    #define BUTTON_A    25 // left button
    #define BUTTON_B    26 // center button
    #define BUTTON_C    27 // right button
#else
    #error Hardware buttons not supported!
#endif


BLENotifications notifications;

void onBLEStateChanged(BLENotifications::State state) {
  switch(state) {
      case BLENotifications::StateConnected:
          Serial.println("StateConnected"); 
          break;

      case BLENotifications::StateDisconnected:
          Serial.println("StateDisconnected"); 
          break; 
  }
}


void onNotificationArrived() {
     Serial.println("Got notification.");   
}


void setup() {
  // Button configuration
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

    Serial.begin(115200);
    while(!Serial) {
        delay(10);
    }

    Serial.println("BLENotifications BLE ANCS on ESP32 Example");
    Serial.println("------------------------------------------");    

    notifications.begin("deviceName");
    notifications.setConnectionStateChangedCallback(onBLEStateChanged);
    notifications.setNotificationCallback(onNotificationArrived);
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
- Notification removed
- Notification Attributes
- Perform Notification Actions (positive or negative)
*/