# ESP32 ANCS Notifications
Easy-to-use Arduino library for interfacing an ESP 32 with Bluetooth LE mobile device notifications.

This library is designed to follow the standard Arduino library style, and be as easy to use and clear as possible for non-programmers (i.e. no lambda functions, threads, new C++ language features, etc.)

## Features

  + Support notifications on your device with a few lines of Arduino code.
  + Easily start re-advertising the ESP 32 device if BLE connection is lost.
  + Apple ANCS notification support, with advanced message details.
  + Use actions to accept or reject incoming calls.


## Installation

You need to install the ESP32 core libraries. If you are getting build errors, it's possible you have an old version of the core libraries without BLE support. The install instructions for your specific OS are here:
https://github.com/espressif/arduino-esp32

Next, put this unzipped library code into your ~/arduino/libraries/ folder, under an ESP32-ANCS-Notifications subfolder:

Or use git:
 ```
 cd ~/arduino/libraries/
 git clone git@github.com:Smartphone-Companions/ESP32-ANCS-Notifications.git
 ```

Then you should see the examples and be able to include the library in your projects with:

```
 #include "esp32notifications.h"
```
 
### OSX Troubleshooting

  - On OSX Catalina you might have trouble getting the "TTGO ESP32 OLED" board, or similar, recognised by Arduino. If so, try installing the driver for the CP2104 USB chip. https://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers
 
 
## Usage

This works like a standard Arduino library. Here's a minimal example:

```
// Create an interface to the BLE notification library at the top of your sketch
BLENotifications notifications;

// Start looking for a device connection
notifications.begin("BLEConnection device name");

// This callback will be called when a Bluetooth LE connection is made or broken.
// You can update the ESP 32's UI or take other action here.
void onBLEStateChanged(BLENotifications::State state) {
  switch(state) {
      case BLENotifications::StateConnected:
          Serial.println("StateConnected - connected to a phone or tablet"); 
          break;

      case BLENotifications::StateDisconnected:
          Serial.println("StateDisconnected - disconnected from a phone or tablet"); 
          notifications.startAdvertising(); 
          break; 
  }
}

// Setup a callback for when a notification arrives
void onNotificationArrived(const ArduinoNotification * notification, Notification * rawData) {
    Serial.println(notification->title);
}

// Register the callback to be informed when a notification arrives
notifications.setConnectionStateChangedCallback(onBLEStateChanged);
notifications.setNotificationCallback(onNotificationArrived);
```

Note that the Espressif BLE libraries are very large, so you may need to increase your partition scheme to "Large" in the Arduino IDE.

See the ble_connection example for a more fully-featured example.




## History / Acknowledgements

Based on the work of CarWatch, Hackwatch, and S-March. This project was created to hide the complicated BLE notification internals behind a standard, easy-to-use Arduino library.

To see a real-world project, https://github.com/jhud/hackwatch uses this library.

