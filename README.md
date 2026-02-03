[![arduino-library-badge](https://www.ardu-badge.com/badge/ESP32%20BLE%20ANCS%20Notifications.svg?)](https://www.ardu-badge.com/ESP32%20BLE%20ANCS%20Notifications/)

# ESP32 ANCS Notifications
Easy-to-use Arduino library for interfacing an ESP 32 with Bluetooth LE mobile device notifications.

This library is designed to follow the standard Arduino library style, and be as easy to use and clear as possible for non-programmers (i.e. no lambda functions, threads, new C++ language features, etc.)

## Features

  + Support notifications on your device with a few lines of Arduino code.
  + Easily start re-advertising the ESP 32 device if BLE connection is lost.
  + Apple ANCS notification support, with advanced message details.
  + Use actions to accept or reject incoming calls.


## Installation

### ESP-32 Core Library
You need to install the ESP32 core libraries. If you are getting build errors, it's possible you have an old version of the core libraries without BLE support. The install instructions for your specific OS are here:
https://github.com/espressif/arduino-esp32

### Using Arduino Library Manager

From Arduino IDE, go to menu *Sketch -> Include Library -> Manage Libraries...*
In Library Manager Window, search *"ANCS"* in the search form then select *"ESP32 BLE ANCS Notifications"*
Click *"Install"* button.

### Manual installation

Download zip file from this repository by selecting the green *"Code"* dropdown at the top of repository, select *"Download ZIP"*
From Arduino IDE, select menu *Sketch -> Include Library -> Add .ZIP Library...*

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

```c
#include "esp32notifications.h"

// Create an interface to the BLE notification library at the top of your sketch
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
          notifications.startAdvertising(); 
          break; 
  }
}

// Setup a callback for when a notification arrives
void onNotificationArrived(const ArduinoNotification * notification, const Notification * rawData) {
    Serial.println(notification->title);
}

void setup() {
  // Start looking for a device connection
  notifications.begin("BLEConnection device name");

  // Register the callback to be informed when a notification arrives
  notifications.setConnectionStateChangedCallback(onBLEStateChanged);
  notifications.setNotificationCallback(onNotificationArrived);
}
```

Note that the Espressif BLE libraries are very large, so you may need to increase your partition scheme to "Large" in the Arduino IDE.

See the `examples/ios_notifications` sketch for a minimal Serial-only example, or `examples/ble_connection` for button-driven call actions.

## iOS Setup

### Discovery (CoreBluetooth Apps)
iOS does not reliably list generic BLE peripherals in the standard Bluetooth Settings screen. Use a CoreBluetooth app such as nRF Connect for discovery.

Recommended discovery/testing flow:
1. Upload `examples/ios_notifications/ios_notifications.ino`.
2. Open a BLE scanner app (nRF Connect) and connect to the ESP32 device name you used in `notifications.begin(...)`.
3. When prompted, accept pairing/bonding and tap “Allow Notifications”.
4. If you disconnect, the example calls `startAdvertising()` to make the device visible again.

### Pairing / Bonding Helper
If iOS does not prompt to pair, use the dedicated helper to force an encrypted characteristic access:
1. Upload `examples/bonding_helper/bonding_helper.ino`.
2. In nRF Connect, connect and READ or WRITE the helper characteristic.
3. Accept the pairing/bonding prompt, then disconnect.
4. Re-flash the ANCS example and connect normally (bonding keys should persist).

### “Show Previews” Behavior
If iOS “Show Previews” is set to **Never** or **When Unlocked**, Apple may send ANCS notifications with empty message bodies. The library still invokes the callback as soon as the app identifier and/or title are available, with `notification->message` left empty.

### Troubleshooting
- If notifications do not appear: iOS Settings → Bluetooth → (i) → “Forget This Device”, then re-pair via nRF Connect.
- Ensure the iOS app has notifications enabled (Settings → Notifications).
- Ensure “Show Previews” allows content if you expect message bodies.




## History / Acknowledgements

Based on the work of CarWatch, Hackwatch, and S-March. This project was created to hide the complicated BLE notification internals behind a standard, easy-to-use Arduino library.

To see a real-world project, https://github.com/jhud/hackwatch uses this library.
