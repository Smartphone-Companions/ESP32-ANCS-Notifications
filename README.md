# ESP32NotificationsLib
Easy-to-use Arduino library for interfacing an ESP 32 with Bluetooth LE mobile device notifications.

## Installation

Put the unzipped library code into your ~/arduino/libraries/ folder, under a ESP32NotificationsLib subfolder:

Or use git:
 ```
 cd ~/arduino/libraries/
 git clone git@github.com:Smartphone-Companions/ESP32NotificationsLib.git
 ```

Then you should see the examples and be able to include the library in your projects with:

```
 #include "esp32notifications.h"
```
 
## Usage

See the ble_connection example.

## History / Acknowledgements

Based on the work of CarWatch, Hackwatch, and S-March. This project was created to hide the complicated BLE notification internals behind a standard, easy-to-use Arduino library.
