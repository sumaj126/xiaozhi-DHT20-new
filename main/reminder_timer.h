#pragma once

#include <functional>
#include <string>
#include <vector>
#include <esp_timer.h>
#include "voice_command_parser.h"

class ReminderTimer {
public:
    ReminderTimer();
    ~ReminderTimer();

    // Set a relative time reminder (seconds from now)
    void SetReminder(int seconds, const std::string& message);
    
    // Set an absolute time reminder
    void SetReminder(int year, int month, int day, int hour, int minute, const std::string& message);
    
    // Set a repeating reminder
    void SetRepeatingReminder(int hour, int minute, const std::vector<int>& weekdays, ReminderType type, const std::string& message);
    
    // Set reminder from schedule
    void SetReminderFromSchedule(const ReminderSchedule& schedule);
    
    void CancelReminder();
    bool IsReminderSet() const;

    // Callback for when reminder triggers
    void OnReminderTriggered(std::function<void(const std::string& message)> callback);

private:
    void CheckTimer();
    void CalculateNextTrigger();
    int CalculateSecondsUntilTime(int hour, int minute, const std::vector<int>& weekdays);

    esp_timer_handle_t reminder_timer_ = nullptr;
    esp_timer_handle_t repeating_check_timer_ = nullptr;
    bool enabled_ = false;
    int seconds_remaining_ = 0;
    std::string reminder_message_;
    
    // For repeating reminders
    ReminderType reminder_type_ = ReminderType::kOnce;
    int reminder_hour_ = 0;
    int reminder_minute_ = 0;
    std::vector<int> reminder_weekdays_;
    int reminder_year_ = 0;
    int reminder_month_ = 0;
    int reminder_day_ = 0;

    std::function<void(const std::string& message)> on_reminder_triggered_;
};
