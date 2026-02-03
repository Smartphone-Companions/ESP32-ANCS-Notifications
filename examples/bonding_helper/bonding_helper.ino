/*
  Bond Helper for iOS + ESP32 (ANCS)

  Usage:
  1) Flash this sketch to the ESP32.
  2) On iPhone, open nRF Connect and scan for "ESP32-BOND-HELPER".
  3) Connect, then find the "Bond Helper" service and READ or WRITE the characteristic.
  4) Accept the pairing/bonding prompt. You should see pairing logs on Serial.
  5) Disconnect.
  6) Flash the ANCS sketch and connect normally (bonding keys should persist).
  7) If it still fails: iOS Settings > Bluetooth > (i) > Forget This Device, then repeat.

  Notes:
  - iOS BLE addresses are private and can change; do NOT hardcode MAC addresses.
  - Bonding uses stored keys, not a stable public address.
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLESecurity.h>

static const char *kDeviceName = "ESP32-BOND-HELPER";
static const char *kServiceUUID = "8e0e5a9d-2c4e-4b7b-9c20-343c0d1b2b7a";
static const char *kCharUUID = "9c8d2c16-1b9a-4f1a-8b7e-2d0a5f7c9d1f";

BLEServer *gServer = nullptr;
BLECharacteristic *gChar = nullptr;
bool gConnected = false;

class BondServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    gConnected = true;
    Serial.println("Client connected");
  }

  void onDisconnect(BLEServer *pServer) override {
    gConnected = false;
    Serial.println("Client disconnected");
    BLEDevice::startAdvertising();
    Serial.println("Advertising restarted");
  }
};

class BondCharCallbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic) override {
    Serial.println("Characteristic read");
  }

  void onWrite(BLECharacteristic *pCharacteristic) override {
    String value = pCharacteristic->getValue();
    Serial.printf("Characteristic write (%u bytes)\n", static_cast<unsigned>(value.length()));
  }
};

class BondSecurityCallbacks : public BLESecurityCallbacks {
  bool onSecurityRequest() override {
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) override {
    if (auth_cmpl.success) {
      Serial.println("Pairing/bonding successful");
    } else {
      Serial.printf("Pairing/bonding failed, status: %d\n", auth_cmpl.fail_reason);
    }
  }
#elif defined(CONFIG_BT_NIMBLE_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
    if (desc->sec_state.encrypted) {
      Serial.println("Pairing/bonding successful");
    } else {
      Serial.println("Pairing/bonding failed");
    }
  }
#endif

  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t pass_key) override { (void)pass_key; }
  bool onConfirmPIN(uint32_t pass_key) override { (void)pass_key; return true; }
};

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  Serial.println("Starting ESP32 Bond Helper");

  BLEDevice::init(kDeviceName);

  BLESecurity *security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  BLEDevice::setSecurityCallbacks(new BondSecurityCallbacks());

  gServer = BLEDevice::createServer();
  gServer->setCallbacks(new BondServerCallbacks());

  BLEService *service = gServer->createService(BLEUUID(kServiceUUID));
  gChar = service->createCharacteristic(
      BLEUUID(kCharUUID),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
  );

  gChar->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  gChar->setValue("bond");
  gChar->setCallbacks(new BondCharCallbacks());

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLEUUID(kServiceUUID));
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("Bond helper is advertising. Use nRF Connect to read/write the characteristic.");
}

void loop() {
  delay(1000);
}
