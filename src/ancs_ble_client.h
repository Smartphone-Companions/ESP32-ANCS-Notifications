#ifndef ANCS_BLE_CLIENT_H_
#define ANCS_BLE_CLIENT_H_

class BLEAddress;
class BLERemoteCharacteristic;


#include "BLEUUID.h"

/**
 * Internal module for creating and managing an ANCS client connection.
 */
// @todo - should definitely be a class
void ancs_ble_client_init(const BLEAddress* address);
void ancs_ble_client_update();
BLEUUID getAncsServiceUUID();

#endif // ANCS_BLE_CLIENT_H_