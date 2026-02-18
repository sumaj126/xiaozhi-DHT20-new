#pragma once

#include <string>

class VoiceCommandParser {
public:
    static bool ParseReminderCommand(const std::string& command, int& minutes, std::string& message);
    static bool ParseDateTimeReminderCommand(const std::string& command, int& year, int& month, int& day, int& hour, int& minute, std::string& message);
};
