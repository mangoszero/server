/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 * @file ConsoleLogWriter.cpp
 * @brief Off-thread console log writer implementation
 *
 * Implements the dedicated thread that renders queued console log records,
 * moving the per-call console flush (and the SetConsoleTextAttribute / OEM
 * transform) off the world and map-update threads.
 */

#include "ConsoleLogWriter.h"

#include <cstdio>

ConsoleLogWriter::ConsoleLogWriter()
    : m_depth(0), m_dropped(0), m_running(true)
{
}

void ConsoleLogWriter::Enqueue(ConsoleLogRecord& rec)
{
    if (m_depth.load() >= MAX_CONSOLE_QUEUE)
    {
        ++m_dropped;
        return;
    }
    ++m_depth;
    m_queue.add(rec);
}

void ConsoleLogWriter::run()
{
    while (m_running)
    {
        if (!DrainOnce())
        {
            MaNGOS::Thread::Sleep(5);
        }
    }
    // Authoritative final drain: runs on the writer thread while wait()
    // blocks the caller, so any straggler enqueued before Stop() is rendered.
    DrainOnce();
}

bool ConsoleLogWriter::DrainOnce()
{
    bool didWork = false;

    long dropped = m_dropped.load();
    if (dropped > 0)
    {
        m_dropped -= dropped;
        char note[96];
        int n = snprintf(note, sizeof(note),
            "[Log] %ld console line(s) dropped (queue full)\n", dropped);
        if (n >= (int)sizeof(note))
        {
            n = (int)sizeof(note) - 1;                       // clamp: never fwrite past note[]
        }
        if (n > 0)
        {
            // A progress-bar redraw may have parked the cursor mid-line; wipe it
            // first so the notice does not land on top of the bar.
            Log::ClearConsoleLine(stdout);
            Log::SetColor(true, RED);
            fwrite(note, 1, (size_t)n, stdout);
            Log::ResetColor(true);
        }
        didWork = true;
    }

    ConsoleLogRecord rec;
    while (m_queue.next(rec))
    {
        --m_depth;
        Emit(rec);
        didWork = true;
    }

    // The writer owns the console stream, so it owns the flush: a redirected /
    // piped stdout is fully buffered, and flushing here (off the world/map
    // threads) makes output prompt without any producer paying for it. A real
    // console is effectively unbuffered, so this is a cheap no-op there.
    if (didWork)
    {
        fflush(stdout);
    }

    return didWork;
}

void ConsoleLogWriter::Emit(const ConsoleLogRecord& rec)
{
    FILE* out = rec.toStdout ? stdout : stderr;
    if (rec.isRaw)
    {
        // Progress-bar redraw (or similar): the producer already baked in the
        // exact bytes including any '\r'/'\n'. Write verbatim with NO color and
        // NO appended newline, or the bar's in-place repaint breaks.
        if (!rec.text.empty())
        {
            fwrite(rec.text.data(), 1, rec.text.size(), out);
            // Anything not terminated by a newline leaves the cursor parked on a
            // drawn line, which the next ordinary log line has to wipe first.
            Log::MarkConsoleLineDirty(rec.text[rec.text.size() - 1] != '\n');
        }
        return;
    }

    // An ordinary line must never be printed on top of a bar / startup-UI line
    // that is still drawn: erase it and start from a clean column 0.
    Log::ClearConsoleLine(out);

    if (rec.applyColor)
    {
        Log::SetColor(rec.toStdout, rec.color);
    }
    if (!rec.text.empty())
    {
        fwrite(rec.text.data(), 1, rec.text.size(), out);
    }
    if (rec.applyColor)
    {
        Log::ResetColor(rec.toStdout);
    }
    // Newline is written AFTER ResetColor so the line terminator stays outside
    // the color span, byte-matching the legacy console ordering. The producer
    // (Log::ConsoleEmit) deliberately leaves it off rec.text.
    fputc('\n', out);
}
