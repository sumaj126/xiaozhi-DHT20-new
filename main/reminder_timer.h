#pragma once

#include <functional>
#include <string>
#include <esp_timer.h>

class ReminderTimer {
public:
    ReminderTimer();
    ~ReminderTimer();

    void SetReminder(int seconds, const std::string& message);
    void CancelReminder();
    bool IsReminderSet() const;

    // Callback for when reminder triggers
    void OnReminderTriggered(std::function<void(const std::string& message)> callback);

private:
    void CheckTimer();

    esp_timer_handle_t reminder_timer_ = nullptr;
    bool enabled_ = false;
    int seconds_remaining_ = 0;
    std::string reminder_message_;

    std::function<void(const std::string& message)> on_reminder_triggered_;
};
