#include "ancs_notification_queue.h"
#include <type_traits>

static char LOG_TAG[] = "ANCSNotificationQueue";


void ANCSNotificationQueue::addPendingNotification(const Notification pending)
{
  pendingNotification.push(pending);
  ESP_LOGD(LOG_TAG, "Add pending Notification, id: %d", pending.uuid);
}


bool ANCSNotificationQueue::pendingNotificationExists() {
	return (pendingNotification.size() > 0);
}


Notification ANCSNotificationQueue::getNextPendingNotification()
{
	assert(pendingNotification.size() > 0);
    Notification pending = pendingNotification.top();
    pendingNotification.pop();
    ESP_LOGD(LOG_TAG, "Getting pending Notification %d", pending.uuid);
    return pending;
}


void ANCSNotificationQueue::addNotification(uint32_t uuid, Notification notification, bool isCalling)
{
  notification.uuid = uuid;
  if (isCalling)
  {
    callingNotification = notification;
  }
  else
  {
    if (notificationList.size() >= notificationListSize)
    {
      std::map<uint32_t, Notification>::iterator it = notificationList.begin();
      notificationList.erase(it);
    }
    notificationList.insert(std::pair<uint32_t, Notification>(uuid, notification));
  }
};


std::map<uint32_t, Notification> *ANCSNotificationQueue::getNotificationList()
{
  return &notificationList;
}


bool ANCSNotificationQueue::contains(uint32_t uuid)
{
  std::map<uint32_t, Notification>::iterator it = notificationList.find(uuid);
  return (it != notificationList.end()) | (callingNotification.uuid != 0);
}


Notification *ANCSNotificationQueue::getNotification(uint32_t uuid)
{
  if (callingNotification.uuid == uuid)
  {
    return &callingNotification;
  }
  std::map<uint32_t, Notification>::iterator it = notificationList.find(uuid);
  return (it != notificationList.end()) ? &it->second : new Notification();
}


void ANCSNotificationQueue::removeCallNotification()
{
  callingNotification.uuid = 0;
}


bool ANCSNotificationQueue::isCallingNotification() {
  return (callingNotification.uuid != 0);
}


Notification *ANCSNotificationQueue::getCallingNotification() {
  return &callingNotification;
}


void ANCSNotificationQueue::removeNotification(uint32_t uuid) {
  std::map<uint32_t, Notification>::iterator it = notificationList.find(uuid);
  if (it != notificationList.end()) notificationList.erase(it);
}
