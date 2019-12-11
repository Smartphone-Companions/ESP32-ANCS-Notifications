// @author James Hudson bugs.feedback.whatever@gmail.com
// @todo license text and header - please see github project for license

// You will need to add these hardware buttons
#define BUTTON_A    4 // left button
#define BUTTON_B    7 // center switch
#define BUTTON_C    5 // right button


BLENotifications notifications;;

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

    notifications.begin()
    notifications.setName("deviceName");
    notifications.setConnectCallback(connect_callback);
    notifications.setDisconnectCallback(disconnect_callback);
    notifications.setNotificationCallback(notification_callback);
}

void notification_callback(AncsNotification_t* notif)
{
    Serial.println("Got notifications");
}


/**
 * Callback invoked when a BLE connection is made.
 */
void connect_callback(uint16_t conn_handle)
{
}


/**
 * Callback invoked when a connection is dropped
 * @param conn_handle
 * @param reason is a BLE_HCI_STATUS_CODE which can be found in ble_hci.h
 */
void disconnect_callback(uint16_t conn_handle, uint8_t reason)
{
  (void) reason;

  Serial.println();
  Serial.print("Disconnected, reason = 0x"); Serial.println(reason, HEX);
}


void loop() {
    if (BLENotifications.numPending == 0) {
        return;
    }

    checkButtons();

    notifications.actionPositive();
    notifications.actionNegative();
}

/*
X- Connect/Disconnect 
X- Set Name of Peripheral
X- Notification added 
- Notification removed
- Notification Attributes
- Perform Notification Actions (positive or negative)
*/
