#include "ScheduledExit.h"

#include <ctime>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int g_failures = 0;

    void Check(bool condition, char const* expression, int line)
    {
        if (!condition)
        {
            std::cerr << "ScheduledExitTest failure at line " << line << ": " << expression << std::endl;
            ++g_failures;
        }
    }

#define CHECK(expr) Check((expr), #expr, __LINE__)

    std::tm MakeLocalTime(int wday, int yday, int hour, int minute)
    {
        std::tm now = std::tm();
        now.tm_wday = wday;
        now.tm_yday = yday;
        now.tm_hour = hour;
        now.tm_min = minute;
        return now;
    }

    void TestTimeParser()
    {
        uint32 hour = 0;
        uint32 minute = 0;

        CHECK(MaNGOS::ParseScheduledExitTime("05:00", hour, minute));
        CHECK(hour == 5);
        CHECK(minute == 0);

        CHECK(!MaNGOS::ParseScheduledExitTime("24:00", hour, minute));
        CHECK(!MaNGOS::ParseScheduledExitTime("05:60", hour, minute));
        CHECK(!MaNGOS::ParseScheduledExitTime("5:00", hour, minute));
        CHECK(!MaNGOS::ParseScheduledExitTime("05:0", hour, minute));
        CHECK(!MaNGOS::ParseScheduledExitTime("0500", hour, minute));
    }

    void TestModeParser()
    {
        MaNGOS::ScheduledExitMode mode = MaNGOS::SCHEDULED_EXIT_MODE_SHUTDOWN;

        CHECK(MaNGOS::ParseScheduledExitMode("restart", mode));
        CHECK(mode == MaNGOS::SCHEDULED_EXIT_MODE_RESTART);

        CHECK(MaNGOS::ParseScheduledExitMode("shutdown", mode));
        CHECK(mode == MaNGOS::SCHEDULED_EXIT_MODE_SHUTDOWN);

        CHECK(!MaNGOS::ParseScheduledExitMode("reboot", mode));
    }

    void TestWarningParser()
    {
        std::vector<std::string> errors;
        std::vector<uint32> warnings = MaNGOS::ParseScheduledExitWarningTimes("900,600,300,60,900,bad,1200", 900, errors);

        CHECK(warnings.size() == 4);
        CHECK(warnings[0] == 900);
        CHECK(warnings[1] == 600);
        CHECK(warnings[2] == 300);
        CHECK(warnings[3] == 60);

        CHECK(errors.size() == 2);
    }

    void TestScheduleFiresOncePerMinute()
    {
        MaNGOS::ScheduledExitSchedule schedule;
        schedule.enabled = true;
        schedule.dayOfWeek = 3;
        schedule.hour = 5;
        schedule.minute = 0;
        schedule.mode = MaNGOS::SCHEDULED_EXIT_MODE_RESTART;

        MaNGOS::ScheduledExitState state;
        CHECK(MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 20, 5, 0), state));
        CHECK(!MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 20, 5, 0), state));
        CHECK(!MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 20, 5, 1), state));
        CHECK(MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 27, 5, 0), state));
    }

    void TestScheduleDoesNotBackfire()
    {
        MaNGOS::ScheduledExitSchedule schedule;
        schedule.enabled = true;
        schedule.dayOfWeek = 3;
        schedule.hour = 5;
        schedule.minute = 0;
        schedule.mode = MaNGOS::SCHEDULED_EXIT_MODE_RESTART;

        MaNGOS::ScheduledExitState state;
        CHECK(!MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 20, 5, 1), state));
    }

    void TestScheduleCanBePrimedForStartupMinute()
    {
        MaNGOS::ScheduledExitSchedule schedule;
        schedule.enabled = true;
        schedule.dayOfWeek = 3;
        schedule.hour = 5;
        schedule.minute = 0;
        schedule.mode = MaNGOS::SCHEDULED_EXIT_MODE_RESTART;

        MaNGOS::ScheduledExitState state;
        CHECK(MaNGOS::MarkScheduledExitHandledIfMatching(schedule, MakeLocalTime(3, 20, 5, 0), state));
        CHECK(!MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 20, 5, 0), state));
        CHECK(MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 27, 5, 0), state));
    }

    void TestSchedulePrimeOnlyBlocksMatchingMinute()
    {
        MaNGOS::ScheduledExitSchedule schedule;
        schedule.enabled = true;
        schedule.dayOfWeek = 3;
        schedule.hour = 5;
        schedule.minute = 0;
        schedule.mode = MaNGOS::SCHEDULED_EXIT_MODE_RESTART;

        MaNGOS::ScheduledExitState state;
        CHECK(!MaNGOS::MarkScheduledExitHandledIfMatching(schedule, MakeLocalTime(3, 20, 4, 59), state));
        CHECK(MaNGOS::CheckAndMarkScheduledExit(schedule, MakeLocalTime(3, 20, 5, 0), state));
    }

    void TestScheduledWarningsAreAdditiveToShutdownCountdown()
    {
        MaNGOS::ScheduledExitCountdownActions scheduledActions = MaNGOS::GetScheduledExitCountdownActions(true);
        CHECK(scheduledActions.sendShutdownTimer);
        CHECK(scheduledActions.sendConfiguredWarnings);

        MaNGOS::ScheduledExitCountdownActions normalActions = MaNGOS::GetScheduledExitCountdownActions(false);
        CHECK(normalActions.sendShutdownTimer);
        CHECK(!normalActions.sendConfiguredWarnings);
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    TestTimeParser();
    TestModeParser();
    TestWarningParser();
    TestScheduleFiresOncePerMinute();
    TestScheduleDoesNotBackfire();
    TestScheduleCanBePrimedForStartupMinute();
    TestSchedulePrimeOnlyBlocksMatchingMinute();
    TestScheduledWarningsAreAdditiveToShutdownCountdown();

    if (g_failures)
    {
        std::cerr << g_failures << " ScheduledExit tests failed." << std::endl;
        return 1;
    }

    std::cout << "ScheduledExit tests passed." << std::endl;
    return 0;
}
