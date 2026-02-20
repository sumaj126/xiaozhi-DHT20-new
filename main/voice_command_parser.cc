#include "voice_command_parser.h"
#include <esp_log.h>
#include <string>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#define TAG "VoiceCommandParser"

int VoiceCommandParser::ParseChineseNumber(const std::string& num_str) {
    static const struct { const char* chinese; int value; } chinese_numbers[] = {
        {"零", 0}, {"一", 1}, {"二", 2}, {"两", 2}, {"三", 3}, {"四", 4},
        {"五", 5}, {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"十", 10},
        {"十一", 11}, {"十二", 12}, {"十三", 13}, {"十四", 14}, {"十五", 15},
        {"十六", 16}, {"十七", 17}, {"十八", 18}, {"十九", 19}, {"二十", 20},
        {"二十一", 21}, {"二十二", 22}, {"二十三", 23}, {"二十四", 24},
        {"二十五", 25}, {"二十六", 26}, {"二十七", 27}, {"二十八", 28},
        {"二十九", 29}, {"三十", 30}, {"三十一", 31},
        {"四十", 40}, {"五十", 50}, {"六十", 60}
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
    
    size_t start_pos = message.find_first_not_of(" \t\n\r。");
    if (start_pos != std::string::npos) {
        message = message.substr(start_pos);
    }
    
    size_t end_pos = message.find_last_not_of(" \t\n\r。");
    if (end_pos != std::string::npos) {
        message = message.substr(0, end_pos + 1);
    } else {
        message.clear();
    }
    
    if (message.empty()) {
        message = "时间到了";
    }
    
    ESP_LOGI(TAG, "Parsed reminder command: %d minutes, message: %s", minutes, message.c_str());
    return true;
}

bool VoiceCommandParser::ParseTimeExpression(const std::string& time_str, int& hour, int& minute) {
    std::string str = time_str;
    
    // Handle "下午" (afternoon), "晚上" (evening)
    bool is_pm = false;
    bool is_am = false;
    if (str.find("下午") != std::string::npos || str.find("晚上") != std::string::npos) {
        is_pm = true;
    }
    if (str.find("上午") != std::string::npos || str.find("早上") != std::string::npos || str.find("早晨") != std::string::npos) {
        is_am = true;
    }
    
    // Handle "中午" (noon)
    if (str.find("中午") != std::string::npos) {
        hour = 12;
        minute = 0;
        return true;
    }
    
    // Handle "半夜" (midnight)
    if (str.find("半夜") != std::string::npos || str.find("凌晨") != std::string::npos) {
        is_am = true;
    }
    
    // Find hour - patterns like "3点", "十五点", "下午3点"
    size_t hour_pos = str.find("点");
    if (hour_pos == std::string::npos) {
        hour_pos = str.find("時");
    }
    
    if (hour_pos != std::string::npos) {
        // Find the number before "点"
        size_t num_start = hour_pos;
        while (num_start > 0) {
            char c = str[num_start - 1];
            if (!((c >= '0' && c <= '9') || (c & 0x80))) break;
            num_start--;
        }
        
        std::string hour_str = str.substr(num_start, hour_pos - num_start);
        hour = ParseChineseNumber(hour_str);
        
        // Adjust for PM
        if (is_pm && hour < 12) {
            hour += 12;
        }
        if (is_am && hour == 12) {
            hour = 0;
        }
        
        // Find minute - patterns like "3点15分", "3点半"
        size_t min_pos = str.find("分", hour_pos);
        if (min_pos != std::string::npos) {
            size_t min_start = hour_pos + 3; // Skip "点"
            while (min_start < min_pos && (str[min_start] == ' ' || str[min_start] == '\t')) {
                min_start++;
            }
            std::string min_str = str.substr(min_start, min_pos - min_start);
            minute = ParseChineseNumber(min_str);
        } else if (str.find("半", hour_pos) != std::string::npos) {
            minute = 30;
        } else if (str.find("刻", hour_pos) != std::string::npos) {
            minute = 15;
        } else {
            minute = 0;
        }
        
        return hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
    }
    
    // Handle simple hour without "点" - like "下午3"
    size_t num_start = 0;
    for (size_t i = str.length(); i > 0; i--) {
        char c = str[i-1];
        if ((c >= '0' && c <= '9') || (c & 0x80)) {
            num_start = i - 1;
            while (num_start > 0) {
                char prev = str[num_start - 1];
                if (!((prev >= '0' && prev <= '9') || (prev & 0x80))) break;
                num_start--;
            }
            break;
        }
    }
    
    if (num_start < str.length()) {
        std::string hour_str = str.substr(num_start);
        hour = ParseChineseNumber(hour_str);
        if (is_pm && hour < 12) hour += 12;
        if (is_am && hour == 12) hour = 0;
        minute = 0;
        return hour >= 0 && hour <= 23;
    }
    
    return false;
}

bool VoiceCommandParser::ParseDateExpression(const std::string& date_str, int& year, int& month, int& day) {
    std::string str = date_str;
    time_t now = time(NULL);
    struct tm* tm_now = localtime(&now);
    
    // Handle "今天"
    if (str.find("今天") != std::string::npos) {
        year = tm_now->tm_year + 1900;
        month = tm_now->tm_mon + 1;
        day = tm_now->tm_mday;
        return true;
    }
    
    // Handle "明天"
    if (str.find("明天") != std::string::npos) {
        time_t tomorrow = now + 24 * 60 * 60;
        struct tm* tm_tomorrow = localtime(&tomorrow);
        year = tm_tomorrow->tm_year + 1900;
        month = tm_tomorrow->tm_mon + 1;
        day = tm_tomorrow->tm_mday;
        return true;
    }
    
    // Handle "后天"
    if (str.find("后天") != std::string::npos) {
        time_t day_after = now + 2 * 24 * 60 * 60;
        struct tm* tm_day_after = localtime(&day_after);
        year = tm_day_after->tm_year + 1900;
        month = tm_day_after->tm_mon + 1;
        day = tm_day_after->tm_mday;
        return true;
    }
    
    // Handle "大后天"
    if (str.find("大后天") != std::string::npos) {
        time_t day_after = now + 3 * 24 * 60 * 60;
        struct tm* tm_day_after = localtime(&day_after);
        year = tm_day_after->tm_year + 1900;
        month = tm_day_after->tm_mon + 1;
        day = tm_day_after->tm_mday;
        return true;
    }
    
    // Handle patterns like "1月15日", "一月十五"
    size_t month_pos = str.find("月");
    if (month_pos != std::string::npos) {
        // Parse month
        size_t month_start = month_pos;
        while (month_start > 0) {
            char c = str[month_start - 1];
            if (!((c >= '0' && c <= '9') || (c & 0x80))) break;
            month_start--;
        }
        std::string month_str = str.substr(month_start, month_pos - month_start);
        month = ParseChineseNumber(month_str);
        
        // Parse day
        size_t day_pos = str.find("日", month_pos);
        if (day_pos == std::string::npos) {
            day_pos = str.find("号", month_pos);
        }
        if (day_pos != std::string::npos) {
            size_t day_start = month_pos + 3;
            while (day_start < day_pos && (str[day_start] == ' ' || str[day_start] == '\t')) {
                day_start++;
            }
            std::string day_str = str.substr(day_start, day_pos - day_start);
            day = ParseChineseNumber(day_str);
        } else {
            day = 1;
        }
        
        // Parse year if present
        size_t year_pos = str.find("年");
        if (year_pos != std::string::npos && year_pos < month_start) {
            size_t year_start = year_pos;
            while (year_start > 0) {
                char c = str[year_start - 1];
                if (!((c >= '0' && c <= '9') || (c & 0x80))) break;
                year_start--;
            }
            std::string year_str = str.substr(year_start, year_pos - year_start);
            year = ParseChineseNumber(year_str);
            if (year < 100) year += 2000;
        } else {
            year = tm_now->tm_year + 1900;
        }
        
        return true;
    }
    
    return false;
}

bool VoiceCommandParser::ParseWeekdays(const std::string& str, std::vector<int>& weekdays) {
    weekdays.clear();
    
    // Handle "工作日"
    if (str.find("工作日") != std::string::npos || str.find("平日") != std::string::npos) {
        weekdays = {1, 2, 3, 4, 5};  // Monday to Friday
        return true;
    }
    
    // Handle "周末"
    if (str.find("周末") != std::string::npos) {
        weekdays = {0, 6};  // Sunday and Saturday
        return true;
    }
    
    // Handle "每天"
    if (str.find("每天") != std::string::npos || str.find("每日") != std::string::npos) {
        weekdays = {0, 1, 2, 3, 4, 5, 6};  // All days
        return true;
    }
    
    // Handle specific weekdays
    static const struct { const char* name; int day; } weekday_names[] = {
        {"周日", 0}, {"星期日", 0}, {"星期天", 0}, {"天", 0},
        {"周一", 1}, {"星期一", 1}, {"一", 1},
        {"周二", 2}, {"星期二", 2}, {"二", 2},
        {"周三", 3}, {"星期三", 3}, {"三", 3},
        {"周四", 4}, {"星期四", 4}, {"四", 4},
        {"周五", 5}, {"星期五", 5}, {"五", 5},
        {"周六", 6}, {"星期六", 6}, {"六", 6}
    };
    
    for (const auto& wd : weekday_names) {
        if (str.find(wd.name) != std::string::npos) {
            if (std::find(weekdays.begin(), weekdays.end(), wd.day) == weekdays.end()) {
                weekdays.push_back(wd.day);
            }
        }
    }
    
    // Handle "到" or "至" for ranges like "周一到周五"
    size_t to_pos = str.find("到");
    if (to_pos == std::string::npos) {
        to_pos = str.find("至");
    }
    
    if (to_pos != std::string::npos && !weekdays.empty()) {
        // Parse the second weekday
        std::string second_part = str.substr(to_pos + 3);
        for (const auto& wd : weekday_names) {
            if (second_part.find(wd.name) != std::string::npos) {
                int start_day = weekdays[0];
                int end_day = wd.day;
                weekdays.clear();
                for (int d = start_day; d <= end_day; d++) {
                    weekdays.push_back(d);
                }
                break;
            }
        }
    }
    
    return !weekdays.empty();
}

bool VoiceCommandParser::ParseDateTimeReminderCommand(const std::string& command, int& year, int& month, int& day, int& hour, int& minute, std::string& message) {
    ESP_LOGI(TAG, "Parsing date time command: %s", command.c_str());
    
    ReminderSchedule schedule;
    if (ParseAdvancedReminderCommand(command, schedule)) {
        year = schedule.year;
        month = schedule.month;
        day = schedule.day;
        hour = schedule.hour;
        minute = schedule.minute;
        message = schedule.message;
        return schedule.type == ReminderType::kOnce;
    }
    
    return false;
}

bool VoiceCommandParser::ParseAdvancedReminderCommand(const std::string& command, ReminderSchedule& schedule) {
    ESP_LOGI(TAG, "Parsing advanced reminder command: %s", command.c_str());
    
    std::string cmd = command;
    
    // Find "提醒" position
    size_t remind_pos = cmd.find("提醒");
    if (remind_pos == std::string::npos) {
        remind_pos = cmd.find("叫");
        if (remind_pos == std::string::npos) {
            remind_pos = cmd.find("通知");
        }
    }
    
    if (remind_pos == std::string::npos) {
        return false;
    }
    
    // Extract message
    size_t me_pos = cmd.find("我", remind_pos);
    size_t msg_start;
    if (me_pos != std::string::npos) {
        msg_start = me_pos + 3;
    } else {
        msg_start = remind_pos + 6;
    }
    schedule.message = cmd.substr(msg_start);
    
    // Trim message
    size_t start_pos = schedule.message.find_first_not_of(" \t\n\r。");
    if (start_pos != std::string::npos) {
        schedule.message = schedule.message.substr(start_pos);
    }
    size_t end_pos = schedule.message.find_last_not_of(" \t\n\r。");
    if (end_pos != std::string::npos) {
        schedule.message = schedule.message.substr(0, end_pos + 1);
    }
    if (schedule.message.empty()) {
        schedule.message = "时间到了";
    }
    
    // Get the part before "提醒" for parsing time/date
    std::string time_part = cmd.substr(0, remind_pos);
    
    // Check for repeat patterns
    if (time_part.find("每天") != std::string::npos || time_part.find("每日") != std::string::npos) {
        schedule.type = ReminderType::kDaily;
        schedule.weekdays = {0, 1, 2, 3, 4, 5, 6};
    } else if (time_part.find("工作日") != std::string::npos || time_part.find("平日") != std::string::npos) {
        schedule.type = ReminderType::kWorkdays;
        schedule.weekdays = {1, 2, 3, 4, 5};
    } else if (time_part.find("周末") != std::string::npos) {
        schedule.type = ReminderType::kWeekends;
        schedule.weekdays = {0, 6};
    } else if (ParseWeekdays(time_part, schedule.weekdays)) {
        schedule.type = ReminderType::kWeekly;
    } else {
        schedule.type = ReminderType::kOnce;
    }
    
    // Parse time
    if (!ParseTimeExpression(time_part, schedule.hour, schedule.minute)) {
        ESP_LOGI(TAG, "Failed to parse time from: %s", time_part.c_str());
        return false;
    }
    
    // Parse date for one-time reminders
    if (schedule.type == ReminderType::kOnce) {
        time_t now = time(NULL);
        struct tm* tm_now = localtime(&now);
        
        if (!ParseDateExpression(time_part, schedule.year, schedule.month, schedule.day)) {
            // If no date specified, use today or tomorrow based on time
            schedule.year = tm_now->tm_year + 1900;
            schedule.month = tm_now->tm_mon + 1;
            schedule.day = tm_now->tm_mday;
            
            // If the time has already passed today, set for tomorrow
            if (schedule.hour < tm_now->tm_hour || 
                (schedule.hour == tm_now->tm_hour && schedule.minute <= tm_now->tm_min)) {
                time_t tomorrow = now + 24 * 60 * 60;
                struct tm* tm_tomorrow = localtime(&tomorrow);
                schedule.year = tm_tomorrow->tm_year + 1900;
                schedule.month = tm_tomorrow->tm_mon + 1;
                schedule.day = tm_tomorrow->tm_mday;
            }
        }
    }
    
    ESP_LOGI(TAG, "Parsed reminder: type=%d, time=%02d:%02d, date=%04d-%02d-%02d, weekdays=%d, message=%s",
             (int)schedule.type, schedule.hour, schedule.minute,
             schedule.year, schedule.month, schedule.day,
             (int)schedule.weekdays.size(), schedule.message.c_str());
    
    return true;
}

ReminderCommandType VoiceCommandParser::ParseReminderManagementCommand(const std::string& command, ReminderSchedule& schedule) {
    ESP_LOGI(TAG, "Parsing reminder management command: %s", command.c_str());
    
    std::string cmd = command;
    
    // Check for "取消所有提醒" or "取消全部提醒"
    if (cmd.find("取消所有") != std::string::npos || 
        cmd.find("取消全部") != std::string::npos ||
        cmd.find("删除所有") != std::string::npos ||
        cmd.find("删除全部") != std::string::npos ||
        cmd.find("清除所有") != std::string::npos ||
        cmd.find("清除全部") != std::string::npos) {
        ESP_LOGI(TAG, "Parsed cancel all reminders command");
        return ReminderCommandType::kCancelAll;
    }
    
    // Check for "查看提醒" or "提醒列表"
    if (cmd.find("查看提醒") != std::string::npos ||
        cmd.find("提醒列表") != std::string::npos ||
        cmd.find("有什么提醒") != std::string::npos ||
        cmd.find("几个提醒") != std::string::npos ||
        cmd.find("多少提醒") != std::string::npos) {
        ESP_LOGI(TAG, "Parsed list reminders command");
        return ReminderCommandType::kList;
    }
    
    // Check for "取消第X个提醒" or "取消提醒X"
    size_t cancel_pos = cmd.find("取消第");
    if (cancel_pos != std::string::npos) {
        // Find the number after "第"
        size_t num_start = cancel_pos + 6;  // "取消第" is 6 bytes in UTF-8
        size_t num_end = num_start;
        while (num_end < cmd.length() && (cmd[num_end] & 0x80)) {
            num_end += 3;  // UTF-8 Chinese character
        }
        if (num_end > num_start) {
            std::string num_str = cmd.substr(num_start, num_end - num_start);
            int id = ParseChineseNumber(num_str);
            if (id > 0) {
                schedule.reminder_id = id;
                ESP_LOGI(TAG, "Parsed cancel reminder by ID: %d", id);
                return ReminderCommandType::kCancelById;
            }
        }
    }
    
    // Check for "取消提醒X" pattern
    cancel_pos = cmd.find("取消提醒");
    if (cancel_pos != std::string::npos) {
        size_t num_start = cancel_pos + 12;  // "取消提醒" is 12 bytes in UTF-8
        size_t num_end = num_start;
        while (num_end < cmd.length() && (cmd[num_end] & 0x80)) {
            num_end += 3;
        }
        if (num_end > num_start) {
            std::string num_str = cmd.substr(num_start, num_end - num_start);
            int id = ParseChineseNumber(num_str);
            if (id > 0) {
                schedule.reminder_id = id;
                ESP_LOGI(TAG, "Parsed cancel reminder by ID: %d", id);
                return ReminderCommandType::kCancelById;
            }
        }
    }
    
    // Check for "取消提醒" (cancel last/current reminder)
    if (cmd.find("取消提醒") != std::string::npos ||
        cmd.find("删除提醒") != std::string::npos ||
        cmd.find("不要提醒") != std::string::npos) {
        ESP_LOGI(TAG, "Parsed cancel reminder command");
        return ReminderCommandType::kCancel;
    }
    
    // Check if it's a set reminder command
    if (cmd.find("提醒") != std::string::npos || 
        cmd.find("叫") != std::string::npos ||
        cmd.find("通知") != std::string::npos) {
        if (ParseAdvancedReminderCommand(cmd, schedule)) {
            return ReminderCommandType::kSet;
        }
    }
    
    return ReminderCommandType::kNone;
}
