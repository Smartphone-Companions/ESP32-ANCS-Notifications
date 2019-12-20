#include "ancs_notification_queue.h"
#include <type_traits>

static char LOG_TAG[] = "ANCSNotificationQueue";

void ANCSNotificationQueue::addPendingNotification(uint32_t uuid)
{
  pendingNotification.push(uuid);
  ESP_LOGI(LOG_TAG, "add pending Notification, id: %d", uuid);
}

uint32_t ANCSNotificationQueue::getNextPendingNotification()
{
  if (pendingNotification.size() > 0)
  {
    uint32_t uuid = pendingNotification.top();
    pendingNotification.pop();
    ESP_LOGI(LOG_TAG, "getting pending Notification %d", uuid);
    return uuid;
  }
  else
  {
    //Serial.println("no pending Notification");
    return 0;
  }
};

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
