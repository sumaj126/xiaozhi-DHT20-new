#include "voice_command_parser.h"
#include <esp_log.h>
#include <string>
#include <cstdlib>

#define TAG "VoiceCommandParser"

static int ParseChineseNumber(const std::string& num_str) {
    static const struct { const char* chinese; int value; } chinese_numbers[] = {
        {"零", 0}, {"一", 1}, {"二", 2}, {"两", 2}, {"三", 3}, {"四", 4},
        {"五", 5}, {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"十", 10},
        {"二十", 20}, {"三十", 30}, {"四十", 40}, {"五十", 50},
        {"六十", 60}, {"七十", 70}, {"八十", 80}, {"九十", 90}
    };
    
    for (const auto& item : chinese_numbers) {
        if (num_str == item.chinese) {
            return item.value;
        }
    }
    
    return atoi(num_str.c_str());
}

bool VoiceCommandParser::ParseReminderCommand(const std::string& command, int& minutes, std::string& message) {
    ESP_LOGI(TAG, "Parsing command: %s", command.c_str());
    
    size_t pos = command.find("分钟");
    if (pos == std::string::npos) {
        pos = command.find("分");
        if (pos == std::string::npos) {
            ESP_LOGI(TAG, "No '分钟' or '分' found in command");
            return false;
        }
    }
    
    size_t num_start = 0;
    for (size_t i = pos; i > 0; i--) {
        char c = command[i-1];
        if (c == ' ' || c == '\t') continue;
        if ((c >= '0' && c <= '9') || (c & 0x80)) {
            num_start = i - 1;
            while (num_start > 0) {
                char prev = command[num_start - 1];
                if (prev == ' ' || prev == '\t') break;
                if (!((prev >= '0' && prev <= '9') || (prev & 0x80))) break;
                num_start--;
            }
            break;
        }
    }
    
    std::string num_str = command.substr(num_start, pos - num_start);
    minutes = ParseChineseNumber(num_str);
    
    if (minutes <= 0) {
        ESP_LOGE(TAG, "Failed to parse number: %s", num_str.c_str());
        return false;
    }
    
    size_t remind_pos = command.find("提醒", pos);
    if (remind_pos == std::string::npos) {
        remind_pos = command.find("叫", pos);
        if (remind_pos == std::string::npos) {
            remind_pos = command.find("通知", pos);
            if (remind_pos == std::string::npos) {
                ESP_LOGI(TAG, "No '提醒', '叫' or '通知' found after time");
                return false;
            }
        }
    }
    
    size_t me_pos = command.find("我", remind_pos);
    size_t msg_start;
    if (me_pos != std::string::npos) {
        msg_start = me_pos + 3;
    } else {
        msg_start = remind_pos + 6;
    }
    
    message = command.substr(msg_start);
    
    // Trim whitespace and punctuation from both ends
    size_t start_pos = message.find_first_not_of(" \t\n\r。");
    if (start_pos != std::string::npos) {
        message = message.substr(start_pos);
    }
    
    size_t end_pos = message.find_last_not_of(" \t\n\r。");
    if (end_pos != std::string::npos) {
        message = message.substr(0, end_pos + 1);
    } else {
        // If all characters are whitespace/punctuation, message is empty
        message.clear();
    }
    
    if (message.empty()) {
        message = "时间到了";
    }
    
    ESP_LOGI(TAG, "Parsed reminder command: %d minutes, message: %s", minutes, message.c_str());
    return true;
}

bool VoiceCommandParser::ParseDateTimeReminderCommand(const std::string& command, int& year, int& month, int& day, int& hour, int& minute, std::string& message) {
    ESP_LOGI(TAG, "Parsing date time command: %s", command.c_str());
    return false;
}
