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
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file StartupUIDemo.cpp
 * @brief The `--console-demo` startup: a synthetic world load, so the console
 *        output can be looked at without a database behind it.
 *
 * It exercises every shape the real startup produces -- the banner, the phase
 * rules, steps with and without a progress bar, a step whose loader logs an error
 * mid-way, the row counts picked up from ">> Loaded N" lines, and the completion
 * panel -- against invented numbers. What it renders is what a real startup
 * renders, because it drives the same StartupUI calls World::SetInitialWorldSettings
 * does; only the data and the delays are fake.
 */

#include "StartupUI.h"

#include "ConsoleStyle.h"
#include "GitRevision.h"
#include "Log.h"
#include "ProgressBar.h"
#include "SystemConfig.h"

#include <chrono>
#include <string>
#include <thread>
#include <vector>

/// Mirrors EXPECTED_MANGOSD_CLIENT_VERSION, which lives in game/ and is therefore
/// out of reach from shared/. Demo cosmetics only.
#define DEMO_CLIENT_VERSION "1.12.x"

namespace
{
    /// Route the demo's bar redraws through the single stdout owner, exactly as
    /// Master does for the real startup.
    void DemoBarSink(char const* bytes, size_t len)
    {
        sLog.ConsoleEmitRaw(std::string(bytes, len));
    }

    void Wait(int ms)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    /**
     * @brief Stand in for a loader: run a progress bar of @p rows over roughly
     *        @p durationMs, the way a table load would.
     */
    void FakeLoad(int rows, int durationMs)
    {
        const int frames = 50;

        BarGoLink bar(rows);
        for (int i = 0; i < rows; ++i)
        {
            bar.step();

            if (rows >= frames && (i % (rows / frames)) == 0)
            {
                Wait(durationMs / frames);
            }
        }
    }
}

void StartupUI::Demo(const std::string& styleMode)
{
    StartupUI::Initialize(styleMode);
    BarGoLink::SetOutputState(true);
    BarGoLink::SetConsoleSink(&DemoBarSink);

    std::vector<Row> banner;
    banner.push_back(Row("build", GitRevision::GetFullRevision()));
    banner.push_back(Row("system", GitRevision::GetRunningSystem()));
    banner.push_back(Row("config", "(none: console demo)"));

    const std::string title = std::string(MANGOS_PACKAGENAME "  ") + ConsoleStyle::G().separator +
                              "  World Daemon  " + ConsoleStyle::G().separator + "  WoW " DEMO_CLIENT_VERSION;

    Panel(title, GitRevision::GetProjectRevision(), banner);
    sLog.outString("Console demo: no database is touched and no world is loaded.");

    BeginPhase("Core data");

    Step("Loading MaNGOS strings...");
    FakeLoad(1180, 250);
    sLog.outString(">> Loaded 1180 MaNGOS strings");

    Step("Initialize DBC data stores...");
    FakeLoad(246, 400);
    sLog.outString(">> Loaded 246 DBC files");

    Step("Packing instances...");
    Wait(120);

    BeginPhase("World data");

    Step("Loading Item Templates...");
    FakeLoad(23417, 700);
    sLog.outString(">> Loaded 23417 item prototypes");

    Step("Loading Creature templates...");
    FakeLoad(28440, 900);
    // A loader that complains half way through: the error must survive, on its
    // own line, without landing on top of the step being drawn.
    sLog.outErrorDb("Table `creature_template` entry 4711 has invalid model id, skipped.");
    sLog.outString(">> Loaded 28440 creature templates");

    Step("Loading Quests...");
    FakeLoad(9312, 500);
    sLog.outString(">> Loaded 9312 quests definitions");

    BeginPhase("World systems");

    Step("Starting Map System");
    Wait(300);

    Step("Starting Game Event system...");
    FakeLoad(112, 200);
    sLog.outString(">> Loaded 112 game events");

    EndPhase();

    std::vector<Row> summary;
    summary.push_back(Row("server", GitRevision::GetProductVersionStr()));
    summary.push_back(Row("database", "(none: console demo)"));
    summary.push_back(Row("clients", DEMO_CLIENT_VERSION));
    summary.push_back(Row("enabled", "Eluna, ScriptDev3, Warden"));
    summary.push_back(Row("disabled", "PlayerBots, Remote Access, SOAP"));

    Panel("World initialization complete", "ready in 3.4s", summary);

    StartupUI::Shutdown();
    BarGoLink::SetConsoleSink(NULL);
}
