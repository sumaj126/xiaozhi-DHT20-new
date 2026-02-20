#include "reminder_timer.h"
#include <esp_log.h>
#include <ctime>
#include <algorithm>
#include <climits>

#define TAG "ReminderTimer"

ReminderTimer::ReminderTimer() {
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            ReminderTimer* timer = static_cast<ReminderTimer*>(arg);
            timer->CheckTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reminder_timer",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&timer_args, &reminder_timer_);
    
    // Create repeating check timer
    esp_timer_create_args_t repeat_args = {
        .callback = [](void* arg) {
            ReminderTimer* timer = static_cast<ReminderTimer*>(arg);
            timer->CalculateNextTrigger();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "repeating_check_timer",
        .skip_unhandled_events = true,
    };
    esp_timer_create(&repeat_args, &repeating_check_timer_);
}

ReminderTimer::~ReminderTimer() {
    if (reminder_timer_) {
        esp_timer_stop(reminder_timer_);
        esp_timer_delete(reminder_timer_);
    }
    if (repeating_check_timer_) {
        esp_timer_stop(repeating_check_timer_);
        esp_timer_delete(repeating_check_timer_);
    }
}

void ReminderTimer::OnReminderTriggered(std::function<void(const std::string& message)> callback) {
    on_reminder_triggered_ = callback;
}

void ReminderTimer::SetReminder(int seconds, const std::string& message) {
    ESP_LOGI(TAG, "SetReminder called: %d seconds, message: %s", seconds, message.c_str());
    
    CancelReminder();
    
    if (seconds <= 0) {
        ESP_LOGE(TAG, "Invalid reminder time: %d seconds", seconds);
        return;
    }
    
    reminder_message_ = message;
    reminder_type_ = ReminderType::kOnce;
    seconds_remaining_ = seconds;
    enabled_ = true;
    
    ESP_LOGI(TAG, "Starting reminder timer for %d seconds", seconds);
    esp_timer_start_once(reminder_timer_, seconds * 1000000ULL);
    ESP_LOGI(TAG, "Reminder timer started successfully");
}

void ReminderTimer::SetReminder(int year, int month, int day, int hour, int minute, const std::string& message) {
    ESP_LOGI(TAG, "SetReminder called: %04d-%02d-%02d %02d:%02d, message: %s", 
             year, month, day, hour, minute, message.c_str());
    
    CancelReminder();
    
    // Calculate seconds until the specified time
    time_t now = time(NULL);
    struct tm target_tm = {0};
    target_tm.tm_year = year - 1900;
    target_tm.tm_mon = month - 1;
    target_tm.tm_mday = day;
    target_tm.tm_hour = hour;
    target_tm.tm_min = minute;
    target_tm.tm_sec = 0;
    target_tm.tm_isdst = -1;
    
    time_t target_time = mktime(&target_tm);
    double diff = difftime(target_time, now);
    
    if (diff <= 0) {
        ESP_LOGE(TAG, "Target time is in the past");
        return;
    }
    
    reminder_message_ = message;
    reminder_type_ = ReminderType::kOnce;
    reminder_year_ = year;
    reminder_month_ = month;
    reminder_day_ = day;
    reminder_hour_ = hour;
    reminder_minute_ = minute;
    seconds_remaining_ = (int)diff;
    enabled_ = true;
    
    ESP_LOGI(TAG, "Starting reminder timer for %d seconds", seconds_remaining_);
    esp_timer_start_once(reminder_timer_, seconds_remaining_ * 1000000ULL);
}

void ReminderTimer::SetRepeatingReminder(int hour, int minute, const std::vector<int>& weekdays, ReminderType type, const std::string& message) {
    ESP_LOGI(TAG, "SetRepeatingReminder called: %02d:%02d, type: %d, weekdays: %d, message: %s",
             hour, minute, (int)type, (int)weekdays.size(), message.c_str());
    
    CancelReminder();
    
    reminder_message_ = message;
    reminder_type_ = type;
    reminder_hour_ = hour;
    reminder_minute_ = minute;
    reminder_weekdays_ = weekdays;
    enabled_ = true;
    
    // Calculate seconds until next trigger
    int seconds = CalculateSecondsUntilTime(hour, minute, weekdays);
    if (seconds < 0) {
        ESP_LOGE(TAG, "Failed to calculate next trigger time");
        return;
    }
    
    seconds_remaining_ = seconds;
    ESP_LOGI(TAG, "Starting repeating reminder timer for %d seconds", seconds);
    esp_timer_start_once(reminder_timer_, seconds * 1000000ULL);
}

void ReminderTimer::SetReminderFromSchedule(const ReminderSchedule& schedule) {
    ESP_LOGI(TAG, "SetReminderFromSchedule called, type: %d", (int)schedule.type);
    
    switch (schedule.type) {
        case ReminderType::kOnce:
            if (schedule.year > 0) {
                SetReminder(schedule.year, schedule.month, schedule.day, 
                           schedule.hour, schedule.minute, schedule.message);
            } else {
                SetReminder(schedule.delay_seconds, schedule.message);
            }
            break;
        case ReminderType::kDaily:
        case ReminderType::kWeekly:
        case ReminderType::kWorkdays:
        case ReminderType::kWeekends:
            SetRepeatingReminder(schedule.hour, schedule.minute, schedule.weekdays, 
                                schedule.type, schedule.message);
            break;
    }
}

int ReminderTimer::CalculateSecondsUntilTime(int hour, int minute, const std::vector<int>& weekdays) {
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    
    int current_weekday = tm_now->tm_wday;  // 0=Sunday
    int current_seconds = tm_now->tm_hour * 3600 + tm_now->tm_min * 60 + tm_now->tm_sec;
    int target_seconds = hour * 3600 + minute * 60;
    
    if (weekdays.empty()) {
        // No weekdays specified, just use the time
        int diff = target_seconds - current_seconds;
        if (diff <= 0) {
            diff += 24 * 3600;  // Add one day
        }
        return diff;
    }
    
    // Find the next matching weekday
    int min_diff = INT_MAX;
    
    for (int wd : weekdays) {
        int day_diff = wd - current_weekday;
        if (day_diff < 0) {
            day_diff += 7;
        }
        
        int total_diff;
        if (day_diff == 0) {
            // Same day
            int time_diff = target_seconds - current_seconds;
            if (time_diff > 60) {  // At least 1 minute in the future
                total_diff = time_diff;
            } else {
                total_diff = 7 * 24 * 3600 + time_diff;  // Next week
            }
        } else {
            total_diff = day_diff * 24 * 3600 + (target_seconds - current_seconds);
        }
        
        if (total_diff < min_diff) {
            min_diff = total_diff;
        }
    }
    
    return min_diff;
}

void ReminderTimer::CalculateNextTrigger() {
    if (reminder_type_ == ReminderType::kOnce || !enabled_) {
        return;
    }
    
    // For repeating reminders, calculate next trigger
    int seconds = CalculateSecondsUntilTime(reminder_hour_, reminder_minute_, reminder_weekdays_);
    if (seconds > 0) {
        seconds_remaining_ = seconds;
        ESP_LOGI(TAG, "Next trigger in %d seconds", seconds);
        esp_timer_start_once(reminder_timer_, seconds * 1000000ULL);
    }
}

void ReminderTimer::CancelReminder() {
    if (reminder_timer_) {
        esp_timer_stop(reminder_timer_);
    }
    if (repeating_check_timer_) {
        esp_timer_stop(repeating_check_timer_);
    }
    enabled_ = false;
    seconds_remaining_ = 0;
    reminder_message_.clear();
    reminder_type_ = ReminderType::kOnce;
    ESP_LOGI(TAG, "Reminder cancelled");
}

bool ReminderTimer::IsReminderSet() const {
    return enabled_;
}

void ReminderTimer::CheckTimer() {
    if (!enabled_) {
        return;
    }
    
    ESP_LOGI(TAG, "Reminder countdown: %d seconds left", seconds_remaining_);
    
    if (seconds_remaining_ <= 0) {
        ESP_LOGI(TAG, "Reminder triggered: %s", reminder_message_.c_str());
        ESP_LOGI(TAG, "Calling reminder callback");
        
        enabled_ = false;
        
        if (on_reminder_triggered_) {
            on_reminder_triggered_(reminder_message_);
        }
        
        // For repeating reminders, schedule next trigger
        if (reminder_type_ != ReminderType::kOnce) {
            ESP_LOGI(TAG, "Scheduling next repeating reminder");
            CalculateNextTrigger();
        }
    }
}
