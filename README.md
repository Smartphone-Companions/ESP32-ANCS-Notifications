# ESP32 ANCS Notifications — ESP32-C3 / Arduino-ESP32 3.x repair

Patched from Smartphone-Companions/ESP32-ANCS-Notifications, upstream commit
`c71030e7f5abe53ee6acbfba2cf08e93b8a24b8b`.

Receives iPhone notification app identifiers, titles, messages, categories, flags,
and removal events. Supports positive/negative actions offered by the iPhone.
The existing `BLENotifications` public API is retained.

## Install and run

1. In Arduino IDE Boards Manager, install **esp32 by Espressif Systems 3.3.11**.
2. In Library Manager, install **NimBLE-Arduino by h2zero 2.5.1**.
   This dependency is required; do not install the development 3.x version.
3. Remove the previous **ESP32 BLE ANCS Notifications** library folder from your
   sketchbook `libraries` directory to avoid duplicate library resolution.
4. Use **Sketch → Include Library → Add .ZIP Library** to install this ZIP.
5. Open **File → Examples → ESP32 BLE ANCS Notifications → ble_connection**.
6. Select your C3 board, or **ESP32C3 Dev Module** for a generic C3.
   The example uses no GPIO buttons and requires no extra wiring.
7. If your board uses the C3's native USB connector, enable **USB CDC On Boot**.
   Boards with a USB-to-UART bridge generally use the UART serial port instead.
8. Upload, then open Serial Monitor at **115200 baud**.
9. On iPhone, open **Settings → Bluetooth** and pair with **ESP32-C3 ANCS**.
   Accept the pairing and notification-sharing prompts. Under the device's
   Bluetooth information page, enable **Share System Notifications** if shown.
10. Generate a new notification from another device/app. Its app, title, message,
    UID, category and flags should print to Serial Monitor.

If you previously paired using the broken firmware, first use **Forget This
Device** on the iPhone and pair again. For persistently mismatched bonds, erase
flash once before uploading; this also clears other settings stored on the ESP32.
Notification content depends on iOS notification/preview permissions. BLE pairing
alone does not prove that ANCS subscription succeeded. Enable **Core Debug Level:
Info** to see the `ANCS subscribed` diagnostic or setup failure details.

## What changed

- Replaced Bluedroid-specific BLE/security calls with **NimBLE-Arduino 2.5.1**.
  Uses the same explicit BLE dependency across Arduino-ESP32 core versions.
- Uses `NimBLEServer::getClient(connectionHandle)` to access the iPhone's GATT
  services over the existing connection. The iPhone remains the GAP central;
  the ESP32 is the GAP peripheral and also a GATT client. No second connection.
- Configures encrypted, bonded headless pairing. It does not advertise a display
  or keyboard that the board does not have. This uses Just Works pairing, without
  numeric-comparison MITM protection.
- Advertises the ANCS solicitation UUID with BLE-only flags; keeps the device name
  in the scan response so the primary advertisement stays within 31 bytes.
- Uses `TaskHandle_t`, an unpinned task, and one persistent worker. Disconnects
  invalidate queued work instead of force-deleting a task inside the BLE callback.
- Checks discovery and subscription failures and requests Data Source subscription
  before Notification Source. Rediscovery occurs on each new connection.
- Corrects little-endian decoding of all four UID bytes.
- Reassembles fragmented ANCS responses, including headers split across packets,
  and waits for every requested attribute even when title/message are empty.
- Requests app identifier, title (up to **256 bytes**), and message (up to
  **1024 bytes**) in one command, with only one attribute response in flight.
- Handles added, modified and removed events. Cancels obsolete in-flight delivery
  when that notification is modified or removed. Unknown removal IDs retain the
  event metadata instead of allocating/leaking an empty notification.
- Uses separate bounded queues for short events (128) and data fragments (24),
  up to 128 pending notifications, and a 32-notification detail cache.
- Initializes all public fields, including `time = 0` (date parsing is not
  implemented). Guards actions before initialization/after disconnect.
- Replaces GPIO25/26/27 button assumptions with a board-independent Serial example
  and a bounded USB Serial startup wait.

## Application rules

- Keep one `BLENotifications` instance alive globally. It owns the BLE subsystem;
  do not also initialize the bundled `BLEDevice` stack or another BLE library.
- Register callbacks before `begin()`. Existing setters also work after startup.
- Connection callbacks run on the Bluetooth task. Keep them brief; set a flag and
  do application work from `loop()`. The example defers re-advertising this way.
- Notification/removal callbacks run on the ANCS worker. Keep them brief too;
  blocking them prevents processing incoming data. Both callback pointers are
  valid **only until the callback returns**; copy fields you need to retain.
- `StateConnected` means a BLE link exists, not that ANCS is ready. The first
  notification or the debug `ANCS subscribed` line confirms progress.
- `actionPositive(uid)` and `actionNegative(uid)` queue commands. Check
  `ANCS::EventFlagPositiveAction` or `ANCS::EventFlagNegativeAction` before offering
  the corresponding action. iOS can still reject a stale or unsupported action.
- Call `begin()`, `startAdvertising()`, callback setters and `stop()` from the
  Arduino setup/loop task. `stop()` returns false if a BLE operation has not yet
  returned; retry from `loop()` rather than destroying the object. Restart after
  successful `stop()` is intentionally unsupported; reboot to initialize again.
- The worker disconnects on malformed/oversized data, response timeout (10 seconds),
  or queue overflow. The example re-advertises after disconnect. This avoids mixing
  a late headerless fragment with the next notification. Extreme bursts exceeding
  the stated bounds can lose notifications; this is not a lossless notification log.
- Only 32 completed notifications retain their text for removal callbacks. Older
  removals still include the UID and the current Notification Source metadata.

## Verification

See [VALIDATION.md](VALIDATION.md) for exact locally tested versions and results.
Actual iPhone pairing, live notification delivery, actions and reconnect endurance
require physical hardware and have **not** been tested here.

The included GitHub Actions workflow compiles the example for C3, original ESP32,
and ESP32-S3 on cores 3.0.7 and 3.3.11. Those CI jobs only run after you push this
code to a repository with Actions enabled; inclusion is not evidence of a CI pass.

Reproduce a C3 build:

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.11 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install NimBLE-Arduino@2.5.1
arduino-cli compile --fqbn esp32:esp32:esp32c3 --library . examples/ble_connection
```

Run the host protocol tests from this library directory:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -Isrc tests/protocol_test.cpp -o /tmp/ancs-protocol-test
/tmp/ancs-protocol-test
```

Hardware acceptance checks:

- Fresh pairing and notification sharing, followed by a normal message.
- Long/UTF-8 text, empty title/body, and notifications from several apps.
- Notifications with different UIDs, modified notifications and removed ones.
- At least 20 disconnect/reconnect cycles while watching available heap.
- Phone/ESP32 reboot with retained bonds; Bluetooth off/on; out-of-range recovery.
- Offered positive/negative notification actions, tested deliberately.

## Sources and license

- [Original repository](https://github.com/Smartphone-Companions/ESP32-ANCS-Notifications)
- [NimBLE-Arduino 2.5.1](https://github.com/h2zero/NimBLE-Arduino/tree/2.5.1)
- [Apple ANCS specification](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleNotificationCenterServiceSpecification/Specification/Specification.html)
- [Arduino-ESP32 migration guide](https://docs.espressif.com/projects/arduino-esp32/en/latest/migration_guides/2.x_to_3.0.html)

Original project and this derivative are GPL-3.0; see LICENSE. Original work by
ECAL / Smartphone Companions and James Hudson, based on CarWatch, Hackwatch and
S-March. NimBLE-Arduino is an independently installed dependency with its own license.
