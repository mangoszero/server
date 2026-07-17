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

#include "StartupUI.h"

#include "ConsoleStyle.h"
#include "Log.h"
#include "ProgressBar.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
    using ConsoleStyle::Accent;
    using ConsoleStyle::Bold;
    using ConsoleStyle::Dim;
    using ConsoleStyle::Fit;
    using ConsoleStyle::Good;
    using ConsoleStyle::Repeat;
    using ConsoleStyle::Reset;
    using ConsoleStyle::VisibleWidth;

    bool        s_stepOpen    = false; ///< a step line is currently drawn
    bool        s_consumeNext = false; ///< the line being logged is ours: file only

    std::string s_stepName;            ///< display name of the open step
    uint64      s_stepStartMs = 0;     ///< when the open step began

    uint32      s_rows        = 0;     ///< rows the step reported via ">> Loaded N"
    bool        s_rowsKnown   = false; ///< whether a ">> Loaded N" line was seen
    uint32      s_barRows     = 0;     ///< rows the step's progress bars announced
    int         s_barDone     = 0;     ///< live bar position
    int         s_barTotal    = 0;     ///< live bar total

    /// Column layout, derived from the terminal width. Everything is sized so the
    /// line stays two columns short of the edge: a wrapped line cannot be
    /// repainted in place.
    struct Layout
    {
        int name;
        int middle;
        int bar;
        int time;
    };

    Layout LayoutFor(int width)
    {
        Layout l;
        l.time = 8;
        l.name = (width >= 96) ? 34 : ((width >= 84) ? 30 : 22);

        l.middle = width - (l.name + l.time + 8);
        if (l.middle < 22)
        {
            l.middle = 22;
        }
        else if (l.middle > 44)
        {
            l.middle = 44;
        }

        // The middle column is the bar plus its readout ("  100%  12.1k/28.4k").
        l.bar = l.middle - 20;
        if (l.bar < 8)
        {
            l.bar = 8;
        }
        else if (l.bar > 24)
        {
            l.bar = 24;
        }

        return l;
    }

    uint64 NowMs()
    {
        using namespace std::chrono;
        return (uint64)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }

    uint32 StepElapsed()
    {
        return (uint32)(NowMs() - s_stepStartMs);
    }

    /// Compact row count: exact while small, then k/M so the column never grows.
    std::string FormatCount(uint32 count)
    {
        char buf[32];

        if (count < 10000)
        {
            snprintf(buf, sizeof(buf), "%u", count);
        }
        else if (count < 1000000)
        {
            snprintf(buf, sizeof(buf), "%.1fk", count / 1000.0);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%.1fM", count / 1000000.0);
        }

        return std::string(buf);
    }

    std::string FormatMs(uint32 ms)
    {
        char buf[32];

        if (ms < 1000)
        {
            snprintf(buf, sizeof(buf), "%ums", ms);
        }
        else if (ms < 60000)
        {
            snprintf(buf, sizeof(buf), "%.1fs", ms / 1000.0);
        }
        else
        {
            snprintf(buf, sizeof(buf), "%um %02us", ms / 60000, (ms % 60000) / 1000);
        }

        return std::string(buf);
    }

    std::string RightAlign(const std::string& text, std::size_t columns)
    {
        const std::size_t visible = VisibleWidth(text);

        if (visible >= columns)
        {
            return text;
        }

        return std::string(columns - visible, ' ') + text;
    }

    /**
     * @brief Turn the logged line into something to show: "Loading Creature
     *        templates..." reads as "Creature templates" once a marker glyph is
     *        sitting in front of it. Lines with another verb ("Packing
     *        instances...") keep it.
     */
    std::string DisplayName(const std::string& text)
    {
        std::string name = text;

        while (!name.empty() && (name[name.size() - 1] == '.' || name[name.size() - 1] == ' '))
        {
            name.erase(name.size() - 1);
        }

        const std::string prefix = "Loading ";
        if (name.size() > prefix.size() && name.compare(0, prefix.size(), prefix) == 0)
        {
            name.erase(0, prefix.size());
        }

        return name;
    }

    /// Erase whatever the previous, possibly longer, repaint left on this line.
    std::string EraseTail(std::size_t drawn)
    {
        if (ConsoleStyle::Colored())
        {
            return "\x1b[K";                                // CSI K: erase to end of line
        }

        const std::size_t edge = (std::size_t)ConsoleStyle::Width() - 1;
        return (drawn < edge) ? std::string(edge - drawn, ' ') : std::string();
    }

    /**
     * @brief Build one step line: marker, name, then either the live bar or the
     *        result, and the elapsed time in a right-aligned column.
     */
    std::string BuildStepLine(const std::string& name, uint32 elapsedMs, bool finished)
    {
        const ConsoleStyle::Glyphs& g = ConsoleStyle::G();
        const Layout l = LayoutFor(ConsoleStyle::Width());

        std::string middle;

        if (finished)
        {
            const uint32 rows = s_rowsKnown ? s_rows : s_barRows;
            if (s_rowsKnown || s_barRows > 0)
            {
                middle = FormatCount(rows) + " rows";
            }
        }
        else if (s_barTotal > 0)
        {
            int filled = (int)((int64)s_barDone * l.bar / s_barTotal);
            if (filled < 0)
            {
                filled = 0;
            }
            else if (filled > l.bar)
            {
                filled = l.bar;
            }

            const int percent = (int)((int64)s_barDone * 100 / s_barTotal);

            char readout[64];
            snprintf(readout, sizeof(readout), "  %3d%%  %s/%s", percent,
                     FormatCount((uint32)s_barDone).c_str(), FormatCount((uint32)s_barTotal).c_str());

            middle = Accent() + Repeat(g.barFull, (std::size_t)filled) +
                     Dim() + Repeat(g.barEmpty, (std::size_t)(l.bar - filled)) +
                     std::string(readout);
        }

        std::string line = "  ";
        line += finished ? (std::string(Good()) + g.done) : (std::string(Accent()) + g.running);
        line += Reset();
        line += " ";
        line += Fit(name, (std::size_t)l.name);
        line += " ";
        line += Dim();
        line += Fit(middle, (std::size_t)l.middle);
        line += " ";
        line += RightAlign(FormatMs(elapsedMs), (std::size_t)l.time);
        line += Reset();

        return line;
    }

    /// Draw the open step's line in place; the cursor stays parked on it.
    void PaintLive()
    {
        if (!ConsoleStyle::Fancy())
        {
            return;
        }

        const std::string line = BuildStepLine(s_stepName, StepElapsed(), false);
        ConsoleStyle::Emit("\r" + line + EraseTail(VisibleWidth(line)));
    }

    /// Replace the open step's line with its result and end the line.
    void PaintDone()
    {
        if (!ConsoleStyle::Fancy())
        {
            return;
        }

        const std::string line = BuildStepLine(s_stepName, StepElapsed(), true);
        ConsoleStyle::Emit("\r" + line + EraseTail(VisibleWidth(line)) + "\n");
    }

    /**
     * @brief Log @p plain to the file and draw @p styled on the console.
     *
     * In plain mode the two are the same line and the ordinary log path prints
     * it. In fancy mode the log line is consumed by the filter (so the file keeps
     * a readable, escape-free record) and the console gets the styled bytes.
     */
    void Paint(const std::string& plain, const std::string& styled)
    {
        if (!ConsoleStyle::Fancy())
        {
            sLog.outString("%s", plain.c_str());
            return;
        }

        s_consumeNext = true;
        sLog.outString("%s", plain.c_str());
        s_consumeNext = false;

        ConsoleStyle::Emit(styled + "\n");
    }

    void CloseStep()
    {
        if (!s_stepOpen)
        {
            return;
        }

        PaintDone();
        s_stepOpen = false;
    }

    /**
     * @brief Console-side line filter (see Log::SetConsoleLineFilter).
     *
     * While a step line is drawn, two kinds of line would spoil it and are folded
     * into the step instead: the ">> Loaded N rows" results, whose count becomes
     * the step's row count, and the blank spacer lines. Everything else -- notes,
     * warnings, anything unexpected -- still prints; the console writer wipes the
     * step line before it does. The log file records all of it either way.
     */
    bool ConsoleFilter(const char* text)
    {
        if (!ConsoleStyle::Fancy())
        {
            return false;
        }

        if (s_consumeNext)
        {
            return true;                                    // our own line: file only
        }

        if (!s_stepOpen)
        {
            return false;
        }

        if (!text || !*text)
        {
            return true;                                    // blank spacer
        }

        if (text[0] != '>')
        {
            return false;
        }

        // ">> Loaded 543 page texts" -> 543. Several loaders report more than one
        // result line per step, so accumulate. Lines with no number at all
        // (">>> Loot Tables loaded") are still consumed, they just add nothing.
        for (const char* p = text; *p; ++p)
        {
            if (*p >= '0' && *p <= '9')
            {
                s_rows += (uint32)strtoul(p, NULL, 10);
                s_rowsKnown = true;
                break;
            }
        }

        return true;
    }

    /**
     * @brief Progress-bar renderer (see BarGoLink::SetRenderer): the bar is drawn
     *        as the middle column of the step line it belongs to.
     */
    std::string RenderBar(int done, int total, uint32 elapsedMs, bool finished)
    {
        if (finished)
        {
            // Last frame: show the bar full. The step's result line replaces it
            // as soon as the step closes.
            s_barDone = total;
        }
        else
        {
            s_barDone = done;
            s_barTotal = total;

            if (done == 0 && total > 0)
            {
                s_barRows += (uint32)total;                 // the bar knows the row count
            }
        }

        // A bar created outside any step (rare, and never during a phase) gets a
        // line of its own rather than being folded into a step that is not there.
        const std::string name    = s_stepOpen ? s_stepName : std::string();
        const uint32      elapsed = s_stepOpen ? StepElapsed() : elapsedMs;

        const std::string line = BuildStepLine(name, elapsed, false);
        std::string bytes = "\r" + line + EraseTail(VisibleWidth(line));

        if (finished && !s_stepOpen)
        {
            bytes += "\n";
        }

        return ConsoleStyle::Encode(bytes);
    }
}

void StartupUI::Initialize(const std::string& styleMode)
{
    ConsoleStyle::Init(styleMode);

    sLog.SetConsoleLineFilter(&ConsoleFilter);

    if (ConsoleStyle::Fancy())
    {
        BarGoLink::SetRenderer(&RenderBar);
    }
}

void StartupUI::Shutdown()
{
    CloseStep();

    BarGoLink::SetRenderer(NULL);
    sLog.SetConsoleLineFilter(NULL);
}

void StartupUI::Step(const std::string& text)
{
    CloseStep();

    // The file log keeps the line exactly as the server has always written it.
    s_consumeNext = ConsoleStyle::Fancy();
    sLog.outString("%s", text.c_str());
    s_consumeNext = false;

    s_stepOpen    = true;
    s_stepName    = DisplayName(text);
    s_stepStartMs = NowMs();
    s_rows        = 0;
    s_rowsKnown   = false;
    s_barRows     = 0;
    s_barDone     = 0;
    s_barTotal    = 0;

    PaintLive();
}

void StartupUI::LogOnly(const std::string& text)
{
    CloseStep();

    s_consumeNext = ConsoleStyle::Fancy();
    sLog.outString("%s", text.c_str());
    s_consumeNext = false;
}

void StartupUI::BeginPhase(const std::string& title)
{
    CloseStep();

    const ConsoleStyle::Glyphs& g = ConsoleStyle::G();
    const int width = ConsoleStyle::Width();

    sLog.outString();

    const std::size_t used = VisibleWidth(title) + 4;
    const std::size_t tail = ((int)used < width - 2) ? (std::size_t)(width - 2) - used : 0;

    const std::string plain  = std::string(g.rule) + std::string(g.rule) + " " + title + " " +
                               Repeat(g.rule, tail);
    const std::string styled = std::string(Dim()) + g.rule + g.rule + " " + Reset() + Bold() + Accent() +
                               title + Reset() + Dim() + " " + Repeat(g.rule, tail) + Reset();

    Paint(plain, styled);
}

void StartupUI::EndPhase()
{
    CloseStep();
}

void StartupUI::Panel(const std::string& title, const std::string& subtitle, const std::vector<Row>& rows)
{
    CloseStep();

    const ConsoleStyle::Glyphs& g = ConsoleStyle::G();

    // Widest key sets the value column; the widest line sets the box.
    std::size_t keyWidth = 8;
    for (std::vector<Row>::const_iterator itr = rows.begin(); itr != rows.end(); ++itr)
    {
        keyWidth = std::max(keyWidth, VisibleWidth(itr->key));
    }

    std::size_t content = std::max(VisibleWidth(title), VisibleWidth(subtitle));
    for (std::vector<Row>::const_iterator itr = rows.begin(); itr != rows.end(); ++itr)
    {
        content = std::max(content, keyWidth + 3 + VisibleWidth(itr->value));
    }

    std::size_t inner = content + 4;                        // two columns of padding each side
    const std::size_t maxInner = (std::size_t)ConsoleStyle::Width() - 2;
    if (inner < 48)
    {
        inner = 48;
    }
    else if (inner > maxInner)
    {
        inner = maxInner;
    }

    const std::size_t body = inner - 4;                     // usable columns between the padding

    sLog.outString();

    // Top edge.
    Paint(std::string(g.boxTopLeft) + Repeat(g.boxHorizontal, inner) + g.boxTopRight,
          std::string(Accent()) + g.boxTopLeft + Repeat(g.boxHorizontal, inner) + g.boxTopRight + Reset());

    // Title, then the optional subtitle.
    Paint(std::string(g.boxVertical) + "  " + Fit(title, body) + "  " + g.boxVertical,
          std::string(Accent()) + g.boxVertical + Reset() + "  " + Bold() + Fit(title, body) + Reset() +
              "  " + Accent() + g.boxVertical + Reset());

    if (!subtitle.empty())
    {
        Paint(std::string(g.boxVertical) + "  " + Fit(subtitle, body) + "  " + g.boxVertical,
              std::string(Accent()) + g.boxVertical + Reset() + "  " + Dim() + Fit(subtitle, body) +
                  Reset() + "  " + Accent() + g.boxVertical + Reset());
    }

    if (!rows.empty())
    {
        Paint(std::string(g.boxTeeLeft) + Repeat(g.rule, inner) + g.boxTeeRight,
              std::string(Accent()) + g.boxTeeLeft + Repeat(g.rule, inner) + g.boxTeeRight + Reset());

        const std::size_t valueWidth = (body > keyWidth + 3) ? (body - keyWidth - 3) : 1;

        for (std::vector<Row>::const_iterator itr = rows.begin(); itr != rows.end(); ++itr)
        {
            const std::string key   = Fit(itr->key, keyWidth);
            const std::string value = Fit(itr->value, valueWidth);

            Paint(std::string(g.boxVertical) + "  " + key + "   " + value + "  " + g.boxVertical,
                  std::string(Accent()) + g.boxVertical + Reset() + "  " + Dim() + key + Reset() + "   " +
                      value + "  " + Accent() + g.boxVertical + Reset());
        }
    }

    // Bottom edge.
    Paint(std::string(g.boxBottomLeft) + Repeat(g.boxHorizontal, inner) + g.boxBottomRight,
          std::string(Accent()) + g.boxBottomLeft + Repeat(g.boxHorizontal, inner) + g.boxBottomRight + Reset());

    sLog.outString();
}
