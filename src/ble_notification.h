#ifndef BLE_NOTIFICATION_H_
#define BLE_NOTIFICATION_H_

/**
 * Notification category, based on ANCS values
 */
typedef enum
{
    CategoryIDOther = 0,
    CategoryIDIncomingCall = 1,
    CategoryIDMissedCall = 2,
    CategoryIDVoicemail = 3,
    CategoryIDSocial = 4,
    CategoryIDSchedule = 5,
    CategoryIDEmail = 6,
    CategoryIDNews = 7,
    CategoryIDHealthAndFitness = 8,
    CategoryIDBusinessAndFinance = 9,
    CategoryIDLocation = 10,
    CategoryIDEntertainment = 11
} NotificationCategory;

/**
 * Callback for when a notification arrives.
 * @param category What sort of notification it is.
 * @param text the text message of the notification. 
 */
typedef void (*ble_notification_arrived_t)(NotificationCategory category, char * text);

#endif