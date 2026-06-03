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

#ifndef MANGOS_SCHEDULED_EXIT_H
#define MANGOS_SCHEDULED_EXIT_H

#include "Platform/Define.h"

#include <ctime>
#include <string>
#include <vector>

namespace MaNGOS
{
    enum ScheduledExitMode
    {
        SCHEDULED_EXIT_MODE_SHUTDOWN = 0,
        SCHEDULED_EXIT_MODE_RESTART
    };

    struct ScheduledExitSchedule
    {
        ScheduledExitSchedule();

        bool enabled;
        uint32 dayOfWeek;
        uint32 hour;
        uint32 minute;
        ScheduledExitMode mode;
    };

    struct ScheduledExitState
    {
        ScheduledExitState();

        bool hasLastFire;
        int lastFireYear;
        int lastFireYearDay;
        int lastFireHour;
        int lastFireMinute;
    };

    struct ScheduledExitCountdownActions
    {
        bool sendShutdownTimer;
        bool sendConfiguredWarnings;
    };

    bool ParseScheduledExitUInt32(std::string const& text, uint32& value);
    bool ParseScheduledExitTime(std::string const& text, uint32& hour, uint32& minute);
    bool ParseScheduledExitMode(std::string const& text, ScheduledExitMode& mode);
    char const* ScheduledExitModeToString(ScheduledExitMode mode);
    std::vector<uint32> ParseScheduledExitWarningTimes(std::string const& text, uint32 delay, std::vector<std::string>& errors);
    ScheduledExitCountdownActions GetScheduledExitCountdownActions(bool scheduledExitCountdownActive);
    bool MarkScheduledExitHandledIfMatching(ScheduledExitSchedule const& schedule, std::tm const& localTime, ScheduledExitState& state);
    bool CheckAndMarkScheduledExit(ScheduledExitSchedule const& schedule, std::tm const& localTime, ScheduledExitState& state);
    std::string BuildScheduledExitWarningText(ScheduledExitMode mode, uint32 remainingSeconds);
}

#endif
