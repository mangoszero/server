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

#ifndef MANGOSSERVER_STARTUPUI_H
#define MANGOSSERVER_STARTUPUI_H

/**
 * @file StartupUI.h
 * @brief The console UI mangosd draws while it brings the world up.
 *
 * World initialization is a long list of loads. Each one is a "step": the step's
 * line is drawn as soon as it starts, repainted in place as its progress bar
 * advances, and finally replaced by a one-line result carrying the row count and
 * how long it took. Steps are grouped into phases, each introduced by a section
 * rule, and the whole thing is bracketed by a banner and a completion panel.
 *
 *   -- World data --------------------------------------------------
 *     » Creature templates   #####-----  42%  12.1k/28.4k      1.2s
 *     v Item templates                        23.4k rows       2.9s
 *
 * Two things make that single-line-per-step shape work without touching the
 * hundreds of managers that do the actual loading:
 *
 * - Row counts arrive on their own. A step picks up the total from the progress
 *   bar the loader creates, and from the ">> Loaded N rows" line the loader
 *   already logs -- which the console filter folds into the step instead of
 *   printing (Log::SetConsoleLineFilter). The log FILE still records every line.
 * - Anything else a loader logs (a warning, a DB error) still prints. The console
 *   writer wipes the half-drawn step line first, so nothing lands on top of it.
 *
 * When stdout is not an interactive console -- redirected to a file, piped, run
 * as a service -- ConsoleStyle turns the whole thing off and every step falls
 * back to the plain "Loading X..." line the server has always printed.
 */

#include "Platform/Define.h"

#include <string>
#include <vector>

namespace StartupUI
{
    /// One key/value line inside a panel.
    struct Row
    {
        Row(const std::string& k, const std::string& v) : key(k), value(v) {}

        std::string key;   ///< left column, dimmed
        std::string value; ///< right column
    };

    /**
     * @brief Probe the terminal, pick the style and install the console hooks
     *        (line filter + progress-bar renderer). Call once, after the config
     *        is loaded and before anything is drawn.
     * @param styleMode the Console.Style setting: "auto", "fancy" or "plain".
     */
    void Initialize(const std::string& styleMode);

    /**
     * @brief Remove the console hooks. Call once world initialization is done, so
     *        no runtime log line and no reload-time progress bar pays for them.
     */
    void Shutdown();

    /**
     * @brief A boxed panel: a title, an optional subtitle, then key/value rows.
     *        Used for both the startup banner and the completion panel.
     */
    void Panel(const std::string& title, const std::string& subtitle, const std::vector<Row>& rows);

    /**
     * @brief Open a phase: draws the section rule that groups the steps under it.
     *        Closes the previous phase's last open step.
     */
    void BeginPhase(const std::string& title);

    /**
     * @brief Close the current phase (and its last step).
     */
    void EndPhase();

    /**
     * @brief Start a step, closing the previous one.
     *
     * @param text the line the server would have logged, verbatim and complete
     *        ("Loading Creature templates..."). It is what the log FILE records,
     *        so nothing about the file log changes; the console shows a tidied
     *        version of it (leading "Loading ", trailing dots removed).
     */
    void Step(const std::string& text);

    /**
     * @brief Record @p text in the log file without drawing it on the console.
     *
     * For the few lines the panels already say better than a log line can, and
     * which ops tooling nonetheless greps for. In plain mode there is no panel,
     * so the line prints as it always has.
     */
    void LogOnly(const std::string& text);

    /**
     * @brief Draw a synthetic world startup -- banner, phases, steps with live
     *        progress bars, an interleaved DB error, completion panel -- against
     *        made-up data, then return.
     *
     * This is how the console output is inspected without standing up a database:
     * `mangosd --console-demo [auto|fancy|plain]`. It is also the quickest way to
     * see what a given terminal, code page or Console.Style setting will actually
     * render, which is the part that cannot be unit-tested.
     *
     * Does its own Initialize()/Shutdown(); the caller only has to have the
     * console writer running.
     *
     * @param styleMode the style to render with, as for Initialize()
     */
    void Demo(const std::string& styleMode);
}

#endif
