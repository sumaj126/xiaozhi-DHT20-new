#include "reminder_timer.h"
#include "application.h"
#include "board.h"
#include "display.h"
#include <esp_log.h>

#define TAG "ReminderTimer"

ReminderTimer::ReminderTimer() {
    ESP_LOGI(TAG, "Creating reminder timer");
    esp_timer_create_args_t timer_args = {
        .callback = [](void* arg) {
            auto self = static_cast<ReminderTimer*>(arg);
            self->CheckTimer();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "reminder_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &reminder_timer_));
    ESP_LOGI(TAG, "Reminder timer created successfully");
}

ReminderTimer::~ReminderTimer() {
    if (reminder_timer_) {
        esp_timer_stop(reminder_timer_);
        esp_timer_delete(reminder_timer_);
    }
}

void ReminderTimer::SetReminder(int seconds, const std::string& message) {
    ESP_LOGI(TAG, "SetReminder called: %d seconds, message: %s", seconds, message.c_str());
    
    if (seconds <= 0) {
        ESP_LOGE(TAG, "Invalid reminder time: %d seconds", seconds);
        return;
    }

    CancelReminder();

    seconds_remaining_ = seconds;
    reminder_message_ = message;
    enabled_ = true;

    ESP_LOGI(TAG, "Starting reminder timer for %d seconds", seconds);
    ESP_ERROR_CHECK(esp_timer_start_periodic(reminder_timer_, 1000000));
    ESP_LOGI(TAG, "Reminder timer started successfully");
}

void ReminderTimer::CancelReminder() {
    if (enabled_) {
        ESP_LOGI(TAG, "Cancelling existing reminder");
        ESP_ERROR_CHECK(esp_timer_stop(reminder_timer_));
        enabled_ = false;
        seconds_remaining_ = 0;
        reminder_message_.clear();
        ESP_LOGI(TAG, "Reminder cancelled");
    }
}

bool ReminderTimer::IsReminderSet() const {
    return enabled_;
}

void ReminderTimer::OnReminderTriggered(std::function<void(const std::string& message)> callback) {
    on_reminder_triggered_ = callback;
    ESP_LOGI(TAG, "Reminder callback set");
}

void ReminderTimer::CheckTimer() {
    if (!enabled_) {
        return;
    }

    seconds_remaining_--;
    
    if (seconds_remaining_ % 10 == 0 || seconds_remaining_ <= 5) {
        ESP_LOGI(TAG, "Reminder countdown: %d seconds left", seconds_remaining_);
    }
    
    if (seconds_remaining_ <= 0) {
        ESP_LOGI(TAG, "Reminder triggered: %s", reminder_message_.c_str());
        enabled_ = false;
        ESP_ERROR_CHECK(esp_timer_stop(reminder_timer_));

        if (on_reminder_triggered_) {
            ESP_LOGI(TAG, "Calling reminder callback");
            on_reminder_triggered_(reminder_message_);
        } else {
            ESP_LOGE(TAG, "No reminder callback set!");
        }

        seconds_remaining_ = 0;
        reminder_message_.clear();
    }
}
