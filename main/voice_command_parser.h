#pragma once

#include <string>
#include <vector>

enum class ReminderType {
    kOnce,              // 一次性提醒
    kDaily,             // 每天重复
    kWeekly,            // 每周重复
    kWorkdays,          // 工作日重复（周一到周五）
    kWeekends           // 周末重复（周六、周日）
};

struct ReminderSchedule {
    ReminderType type = ReminderType::kOnce;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    std::vector<int> weekdays;  // 0=周日, 1=周一, ..., 6=周六
    std::string message;
    int delay_seconds = 0;  // For relative time reminders
};

class VoiceCommandParser {
public:
    // Parse relative time reminder (e.g., "5 minutes later remind me...")
    static bool ParseReminderCommand(const std::string& command, int& minutes, std::string& message);
    
    // Parse absolute time reminder (e.g., "3pm remind me...", "tomorrow 8am remind me...")
    static bool ParseDateTimeReminderCommand(const std::string& command, int& year, int& month, int& day, int& hour, int& minute, std::string& message);
    
    // Parse advanced reminder with repeat options
    static bool ParseAdvancedReminderCommand(const std::string& command, ReminderSchedule& schedule);
    
private:
    static bool ParseTimeExpression(const std::string& time_str, int& hour, int& minute);
    static bool ParseDateExpression(const std::string& date_str, int& year, int& month, int& day);
    static bool ParseWeekdays(const std::string& str, std::vector<int>& weekdays);
    static int ParseChineseNumber(const std::string& num_str);
};
