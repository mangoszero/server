/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "ScheduledExit.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <set>
#include <sstream>

namespace MaNGOS
{
    namespace
    {
        std::string Trim(std::string const& text)
        {
            std::string::size_type first = text.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return "";
            }

            std::string::size_type last = text.find_last_not_of(" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char c) { return char(std::tolower(c)); });
            return text;
        }

        bool IsScheduledExitMinute(ScheduledExitSchedule const& schedule,
            std::tm const& localTime)
        {
            return schedule.enabled &&
                localTime.tm_wday == int(schedule.dayOfWeek) &&
                localTime.tm_hour == int(schedule.hour) &&
                localTime.tm_min == int(schedule.minute);
        }

        void MarkScheduledExitHandled(std::tm const& localTime, ScheduledExitState& state)
        {
            state.hasLastFire = true;
            state.lastFireYear = localTime.tm_year;
            state.lastFireYearDay = localTime.tm_yday;
            state.lastFireHour = localTime.tm_hour;
            state.lastFireMinute = localTime.tm_min;
        }
    }

    ScheduledExitSchedule::ScheduledExitSchedule()
        : enabled(false),
          dayOfWeek(3),
          hour(5),
          minute(0),
          mode(SCHEDULED_EXIT_MODE_RESTART)
    {
    }

    ScheduledExitState::ScheduledExitState()
        : hasLastFire(false),
          lastFireYear(-1),
          lastFireYearDay(-1),
          lastFireHour(-1),
          lastFireMinute(-1)
    {
    }

    bool ParseScheduledExitUInt32(std::string const& text, uint32& value)
    {
        std::string trimmed = Trim(text);
        if (trimmed.empty())
        {
            return false;
        }

        char* end = NULL;
        unsigned long parsed = std::strtoul(trimmed.c_str(), &end, 10);
        if (!end || *end != '\0')
        {
            return false;
        }

        value = uint32(parsed);
        return true;
    }

    bool ParseScheduledExitTime(std::string const& text, uint32& hour, uint32& minute)
    {
        std::string trimmed = Trim(text);
        if (trimmed.size() != 5 || trimmed[2] != ':')
        {
            return false;
        }

        if (!std::isdigit((unsigned char)trimmed[0]) || !std::isdigit((unsigned char)trimmed[1]) ||
            !std::isdigit((unsigned char)trimmed[3]) || !std::isdigit((unsigned char)trimmed[4]))
        {
            return false;
        }

        uint32 parsedHour = uint32((trimmed[0] - '0') * 10 + (trimmed[1] - '0'));
        uint32 parsedMinute = uint32((trimmed[3] - '0') * 10 + (trimmed[4] - '0'));

        if (parsedHour > 23 || parsedMinute > 59)
        {
            return false;
        }

        hour = parsedHour;
        minute = parsedMinute;
        return true;
    }

    bool ParseScheduledExitMode(std::string const& text, ScheduledExitMode& mode)
    {
        std::string lowered = ToLower(Trim(text));
        if (lowered == "shutdown")
        {
            mode = SCHEDULED_EXIT_MODE_SHUTDOWN;
            return true;
        }

        if (lowered == "restart")
        {
            mode = SCHEDULED_EXIT_MODE_RESTART;
            return true;
        }

        return false;
    }

    char const* ScheduledExitModeToString(ScheduledExitMode mode)
    {
        return mode == SCHEDULED_EXIT_MODE_RESTART ? "restart" : "shutdown";
    }

    std::vector<uint32> ParseScheduledExitWarningTimes(
        std::string const& text, uint32 delay, std::vector<std::string>& errors)
    {
        std::vector<uint32> warnings;
        std::set<uint32> seen;
        std::stringstream stream(text);
        std::string token;

        while (std::getline(stream, token, ','))
        {
            std::string trimmed = Trim(token);
            uint32 warningTime = 0;
            if (!ParseScheduledExitUInt32(trimmed, warningTime))
            {
                std::stringstream message;
                message << "invalid warning milestone '" << trimmed << "'";
                errors.push_back(message.str());
                continue;
            }

            if (warningTime > delay)
            {
                std::stringstream message;
                message << "warning milestone " << warningTime
                        << " is greater than delay " << delay;
                errors.push_back(message.str());
                continue;
            }

            if (seen.insert(warningTime).second)
            {
                warnings.push_back(warningTime);
            }
        }

        std::sort(warnings.begin(), warnings.end(), std::greater<uint32>());
        return warnings;
    }

    ScheduledExitCountdownActions GetScheduledExitCountdownActions(
        bool scheduledExitCountdownActive)
    {
        ScheduledExitCountdownActions actions;
        actions.sendShutdownTimer = true;
        actions.sendConfiguredWarnings = scheduledExitCountdownActive;
        return actions;
    }

    bool MarkScheduledExitHandledIfMatching(
        ScheduledExitSchedule const& schedule, std::tm const& localTime,
        ScheduledExitState& state)
    {
        if (!IsScheduledExitMinute(schedule, localTime))
        {
            return false;
        }

        MarkScheduledExitHandled(localTime, state);
        return true;
    }

    bool CheckAndMarkScheduledExit(
        ScheduledExitSchedule const& schedule, std::tm const& localTime,
        ScheduledExitState& state)
    {
        if (!IsScheduledExitMinute(schedule, localTime))
        {
            return false;
        }

        if (state.hasLastFire &&
            state.lastFireYear == localTime.tm_year &&
            state.lastFireYearDay == localTime.tm_yday &&
            state.lastFireHour == localTime.tm_hour &&
            state.lastFireMinute == localTime.tm_min)
        {
            return false;
        }

        MarkScheduledExitHandled(localTime, state);
        return true;
    }

}
