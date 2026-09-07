# Validation of the repaired library

Upstream base: `c71030e7f5abe53ee6acbfba2cf08e93b8a24b8b`.
BLE dependency: official **NimBLE-Arduino 2.5.1** tag
`5ae2c43f62c5a588a25c1de68f7d17b6f3b75adb`.
Target example: `examples/ble_connection/ble_connection.ino`.

## Actual local compile results

| Target | Arduino-ESP32 | Result | Sketch flash | Static global RAM |
|---|---|---|---:|---:|
| `esp32:esp32:esp32c3` | 3.3.11 | PASS, linked firmware binary | 587,163 bytes | 22,564 bytes |
| `esp32:esp32:esp32c3` | 3.0.7 | PASS, linked firmware binary | 535,710 bytes | 21,900 bytes |

These are real cross-compiles using the official RISC-V toolchains and SDK
libraries. Flash figures use the default board partition layout. Static RAM
figures exclude dynamic BLE allocations, notification strings, queues and task
stacks; they are not measurements of runtime heap usage.

The first compile identified that the latest C3 core uses NimBLE, invalidating the
original Bluedroid callback/types. The final source uses the independently pinned
NimBLE-Arduino API and does not include the bundled BLE library.

## Host protocol tests

**14,660 cases passed**, with AddressSanitizer and UndefinedBehaviorSanitizer.
LeakSanitizer was disabled for the local run because this execution environment
prevents its process/thread inspection; no leak-check pass is claimed.

Coverage includes:

- UIDs 0, 1, 0x12345678 and 0xFFFFFFFF, including little-endian reconstruction.
- Every two-fragment split point for the valid fixtures.
- Fragment sizes 1, 7, 20, 182, 244 and 512 bytes.
- Empty values, UTF-8 data and a 1024-byte message.
- Maximum accepted app identifier, title and message lengths together.
- Incorrect UID, command, attribute order, oversized length and trailing data.
- 10,000 deterministic malformed-input stress cases.

`git diff --check` also passed.

## Not verified

No ESP32-C3 or iPhone was attached to this environment. The following remain
hardware acceptance tests, not established results:

- Pairing and iOS notification-sharing authorization.
- Live notification reception, action handling and long-message delivery.
- Reconnection, radio interference, burst behavior and heap stability over time.
- Original ESP32 and ESP32-S3 compilation: included in CI, not run locally.
- Arduino-ESP32 2.x and future core/dependency releases.

The supplied GitHub Actions workflow has not been executed remotely.
