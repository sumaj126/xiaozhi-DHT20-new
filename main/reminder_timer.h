#pragma once

#include <functional>
#include <string>
#include <vector>
#include <map>
#include <esp_timer.h>
#include "voice_command_parser.h"

#define MAX_REMINDERS 10

struct ReminderItem {
    int id;
    ReminderType type;
    int year, month, day;
    int hour, minute;
    std::vector<int> weekdays;
    std::string message;
    esp_timer_handle_t timer;
    bool enabled;
};

class ReminderTimer {
public:
    ReminderTimer();
    ~ReminderTimer();

    // Set a relative time reminder (seconds from now)
    int SetReminder(int seconds, const std::string& message);
    
    // Set an absolute time reminder
    int SetReminder(int year, int month, int day, int hour, int minute, const std::string& message);
    
    // Set a repeating reminder
    int SetRepeatingReminder(int hour, int minute, const std::vector<int>& weekdays, ReminderType type, const std::string& message);
    
    // Set reminder from schedule
    int SetReminderFromSchedule(const ReminderSchedule& schedule);
    
    // Cancel a specific reminder by ID
    bool CancelReminder(int id);
    
    // Cancel all reminders
    void CancelAllReminders();
    
    // Get reminder count
    int GetReminderCount() const;
    
    // Get all reminders
    const std::map<int, ReminderItem>& GetAllReminders() const;
    
    // Get reminder by ID
    const ReminderItem* GetReminder(int id) const;
    
    // Check if any reminder is set
    bool HasReminders() const;

    // Callback for when reminder triggers
    void OnReminderTriggered(std::function<void(const std::string& message, int id)> callback);

private:
    void TriggerReminder(int id);
    void CalculateNextTrigger(int id);
    int CalculateSecondsUntilTime(int hour, int minute, const std::vector<int>& weekdays);
    int GenerateId();
    void StartTimerForReminder(ReminderItem& reminder, int seconds);

    std::map<int, ReminderItem> reminders_;
    int next_id_ = 1;
    
    std::function<void(const std::string& message, int id)> on_reminder_triggered_;
};
