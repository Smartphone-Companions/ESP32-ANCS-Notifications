#ifndef BLE_COMPAT_H_
#define BLE_COMPAT_H_

#include "BLEDevice.h"
#include "BLESecurity.h"
#include "ble_security.h"

namespace ble_compat
{
static inline void installSecurityCallbacks()
{
	BLEDevice::setSecurityCallbacks(new NotificationSecurityCallbacks());
}

template <typename T>
static auto setEncryptionLevelIfSupported(T level, int) -> decltype(BLESecurity::setEncryptionLevel(level), void())
{
	BLESecurity::setEncryptionLevel(level);
}

template <typename T>
static void setEncryptionLevelIfSupported(T, ...)
{
}

static inline void configureSecurityServer(BLESecurity *security)
{
	security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
	security->setCapability(ESP_IO_CAP_OUT);
	security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
#if defined(CONFIG_BLUEDROID_ENABLED)
	setEncryptionLevelIfSupported(ESP_BLE_SEC_ENCRYPT, 0);
#endif
}

static inline void configureSecurityClient(BLESecurity *security)
{
	security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
	security->setCapability(ESP_IO_CAP_IO);
	security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
#if defined(CONFIG_BLUEDROID_ENABLED)
	setEncryptionLevelIfSupported(ESP_BLE_SEC_ENCRYPT, 0);
#endif
}

template <typename T>
static auto secureConnectionIfSupported(T *client, int) -> decltype(client->secureConnection(), bool())
{
	return client->secureConnection();
}

template <typename T>
static bool secureConnectionIfSupported(T *, ...)
{
	return true;
}
} // namespace ble_compat

#endif // BLE_COMPAT_H_
