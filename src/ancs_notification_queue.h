#ifndef NOTIFICATION_SERVICE_H_
#define NOTIFICATION_SERVICE_H_
#include <Arduino.h>
#include <string>
#include <map>
#include <vector>
#include <stack>
#include <time.h>


#include "ble_notification.h"

/**
 * A variable-length queue of notifications.
 * Also contains a special "calling notification", which represents an incoming call.
 */
class ANCSNotificationQueue{
    public:		
        void addPendingNotification(const Notification pending);
		bool pendingNotificationExists();
        Notification getNextPendingNotification();
		
		/**
		 * Add notification to the queue.
		 */
        void addNotification(uint32_t uuid, Notification notification, bool isCalling);
        void removeNotification(uint32_t uuid);
        void removeCallNotification();
		
		/**
		 * Is notification in queue.
		 * @return true if in queue.
		 */
        bool contains(uint32_t uuid);
        bool isCallingNotification();
        Notification *getCallingNotification();
        Notification *getNotification(uint32_t uuid);
        std::map<uint32_t, Notification> *getNotificationList();
        
    private:
        static const int notificationListSize = 32;
        std::map<uint32_t, Notification> notificationList;
        std::stack<Notification> pendingNotification; /**< Goes into a stack first, before we fetch detailed info into the queue. */
        Notification callingNotification;
};
#endif
