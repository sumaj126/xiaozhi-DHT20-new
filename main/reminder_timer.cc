#include "reminder_timer.h"
#include <esp_log.h>
#include <ctime>
#include <algorithm>
#include <climits>

#define TAG "ReminderTimer"

ReminderTimer::ReminderTimer() {
}

ReminderTimer::~ReminderTimer() {
    CancelAllReminders();
}

void ReminderTimer::OnReminderTriggered(std::function<void(const std::string& message, int id)> callback) {
    on_reminder_triggered_ = callback;
}

int ReminderTimer::GenerateId() {
    return next_id_++;
}

int ReminderTimer::SetReminder(int seconds, const std::string& message) {
    ESP_LOGI(TAG, "SetReminder called: %d seconds, message: %s", seconds, message.c_str());
    
    if (seconds <= 0) {
        ESP_LOGE(TAG, "Invalid reminder time: %d seconds", seconds);
        return -1;
    }
    
    if (reminders_.size() >= MAX_REMINDERS) {
        ESP_LOGW(TAG, "Maximum reminders reached (%d)", MAX_REMINDERS);
        return -1;
    }
    
    int id = GenerateId();
    ReminderItem reminder;
    reminder.id = id;
    reminder.type = ReminderType::kOnce;
    reminder.message = message;
    reminder.enabled = true;
    reminder.timer = nullptr;
    
    // Calculate target time
    time_t now = time(NULL);
    time_t target = now + seconds;
    struct tm* tm_target = localtime(&target);
    reminder.year = tm_target->tm_year + 1900;
    reminder.month = tm_target->tm_mon + 1;
    reminder.day = tm_target->tm_mday;
    reminder.hour = tm_target->tm_hour;
    reminder.minute = tm_target->tm_min;
    
    reminders_[id] = reminder;
    StartTimerForReminder(reminders_[id], seconds);
    
    ESP_LOGI(TAG, "Reminder %d created, total: %d", id, (int)reminders_.size());
    return id;
}

int ReminderTimer::SetReminder(int year, int month, int day, int hour, int minute, const std::string& message) {
    ESP_LOGI(TAG, "SetReminder called: %04d-%02d-%02d %02d:%02d, message: %s", 
             year, month, day, hour, minute, message.c_str());
    
    if (reminders_.size() >= MAX_REMINDERS) {
        ESP_LOGW(TAG, "Maximum reminders reached (%d)", MAX_REMINDERS);
        return -1;
    }
    
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
        return -1;
    }
    
    int id = GenerateId();
    ReminderItem reminder;
    reminder.id = id;
    reminder.type = ReminderType::kOnce;
    reminder.year = year;
    reminder.month = month;
    reminder.day = day;
    reminder.hour = hour;
    reminder.minute = minute;
    reminder.message = message;
    reminder.enabled = true;
    reminder.timer = nullptr;
    
    reminders_[id] = reminder;
    StartTimerForReminder(reminders_[id], (int)diff);
    
    ESP_LOGI(TAG, "Reminder %d created, total: %d", id, (int)reminders_.size());
    return id;
}

int ReminderTimer::SetRepeatingReminder(int hour, int minute, const std::vector<int>& weekdays, ReminderType type, const std::string& message) {
    ESP_LOGI(TAG, "SetRepeatingReminder called: %02d:%02d, type: %d, weekdays: %d, message: %s",
             hour, minute, (int)type, (int)weekdays.size(), message.c_str());
    
    if (reminders_.size() >= MAX_REMINDERS) {
        ESP_LOGW(TAG, "Maximum reminders reached (%d)", MAX_REMINDERS);
        return -1;
    }
    
    int seconds = CalculateSecondsUntilTime(hour, minute, weekdays);
    if (seconds < 0) {
        ESP_LOGE(TAG, "Failed to calculate next trigger time");
        return -1;
    }
    
    int id = GenerateId();
    ReminderItem reminder;
    reminder.id = id;
    reminder.type = type;
    reminder.hour = hour;
    reminder.minute = minute;
    reminder.weekdays = weekdays;
    reminder.message = message;
    reminder.enabled = true;
    reminder.timer = nullptr;
    
    reminders_[id] = reminder;
    StartTimerForReminder(reminders_[id], seconds);
    
    ESP_LOGI(TAG, "Repeating reminder %d created, total: %d", id, (int)reminders_.size());
    return id;
}

int ReminderTimer::SetReminderFromSchedule(const ReminderSchedule& schedule) {
    ESP_LOGI(TAG, "SetReminderFromSchedule called, type: %d", (int)schedule.type);
    
    switch (schedule.type) {
        case ReminderType::kOnce:
            if (schedule.year > 0) {
                return SetReminder(schedule.year, schedule.month, schedule.day, 
                           schedule.hour, schedule.minute, schedule.message);
            } else {
                return SetReminder(schedule.delay_seconds, schedule.message);
            }
        case ReminderType::kDaily:
        case ReminderType::kWeekly:
        case ReminderType::kWorkdays:
        case ReminderType::kWeekends:
            return SetRepeatingReminder(schedule.hour, schedule.minute, schedule.weekdays, 
                                schedule.type, schedule.message);
    }
    return -1;
}

void ReminderTimer::StartTimerForReminder(ReminderItem& reminder, int seconds) {
    // Create timer for this reminder
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            ReminderTimer* timer = static_cast<ReminderTimer*>(arg);
            // We need to find which reminder triggered
            // This will be handled by checking all reminders
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reminder_timer",
        .skip_unhandled_events = true,
    };
    
    if (reminder.timer) {
        esp_timer_stop(reminder.timer);
        esp_timer_delete(reminder.timer);
    }
    
    // Create a timer with the reminder ID as argument
    struct TimerArg {
        ReminderTimer* timer;
        int reminder_id;
    };
    
    TimerArg* timer_arg = new TimerArg{this, reminder.id};
    
    esp_timer_create_args_t args = {
        .callback = [](void* arg) {
            TimerArg* ta = static_cast<TimerArg*>(arg);
            ta->timer->TriggerReminder(ta->reminder_id);
            delete ta;
        },
        .arg = timer_arg,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reminder_timer",
        .skip_unhandled_events = true,
    };
    
    esp_timer_create(&args, &reminder.timer);
    esp_timer_start_once(reminder.timer, seconds * 1000000ULL);
    
    ESP_LOGI(TAG, "Timer started for reminder %d, %d seconds", reminder.id, seconds);
}

int ReminderTimer::CalculateSecondsUntilTime(int hour, int minute, const std::vector<int>& weekdays) {
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    
    int current_weekday = tm_now->tm_wday;
    int current_seconds = tm_now->tm_hour * 3600 + tm_now->tm_min * 60 + tm_now->tm_sec;
    int target_seconds = hour * 3600 + minute * 60;
    
    if (weekdays.empty()) {
        int diff = target_seconds - current_seconds;
        if (diff <= 60) {
            diff += 24 * 3600;
        }
        return diff;
    }
    
    int min_diff = INT_MAX;
    
    for (int wd : weekdays) {
        int day_diff = wd - current_weekday;
        if (day_diff < 0) {
            day_diff += 7;
        }
        
        int total_diff;
        if (day_diff == 0) {
            int time_diff = target_seconds - current_seconds;
            if (time_diff > 60) {
                total_diff = time_diff;
            } else {
                total_diff = 7 * 24 * 3600 + time_diff;
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

void ReminderTimer::TriggerReminder(int id) {
    auto it = reminders_.find(id);
    if (it == reminders_.end()) {
        ESP_LOGW(TAG, "Reminder %d not found", id);
        return;
    }
    
    ReminderItem& reminder = it->second;
    ESP_LOGI(TAG, "Reminder %d triggered: %s", id, reminder.message.c_str());
    
    if (on_reminder_triggered_) {
        on_reminder_triggered_(reminder.message, id);
    }
    
    // For repeating reminders, schedule next trigger
    if (reminder.type != ReminderType::kOnce) {
        ESP_LOGI(TAG, "Scheduling next trigger for repeating reminder %d", id);
        int seconds = CalculateSecondsUntilTime(reminder.hour, reminder.minute, reminder.weekdays);
        if (seconds > 0) {
            StartTimerForReminder(reminder, seconds);
        }
    } else {
        // Remove one-time reminder after triggering
        if (reminder.timer) {
            esp_timer_delete(reminder.timer);
        }
        reminders_.erase(it);
        ESP_LOGI(TAG, "One-time reminder %d removed, remaining: %d", id, (int)reminders_.size());
    }
}

bool ReminderTimer::CancelReminder(int id) {
    auto it = reminders_.find(id);
    if (it == reminders_.end()) {
        ESP_LOGW(TAG, "Reminder %d not found", id);
        return false;
    }
    
    ReminderItem& reminder = it->second;
    if (reminder.timer) {
        esp_timer_stop(reminder.timer);
        esp_timer_delete(reminder.timer);
    }
    
    reminders_.erase(it);
    ESP_LOGI(TAG, "Reminder %d cancelled, remaining: %d", id, (int)reminders_.size());
    return true;
}

void ReminderTimer::CancelAllReminders() {
    for (auto& pair : reminders_) {
        if (pair.second.timer) {
            esp_timer_stop(pair.second.timer);
            esp_timer_delete(pair.second.timer);
        }
    }
    reminders_.clear();
    ESP_LOGI(TAG, "All reminders cancelled");
}

int ReminderTimer::GetReminderCount() const {
    return reminders_.size();
}

const std::map<int, ReminderItem>& ReminderTimer::GetAllReminders() const {
    return reminders_;
}

const ReminderItem* ReminderTimer::GetReminder(int id) const {
    auto it = reminders_.find(id);
    if (it != reminders_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool ReminderTimer::HasReminders() const {
    return !reminders_.empty();
}
