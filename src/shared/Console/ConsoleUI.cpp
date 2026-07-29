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

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include "ConsoleUI.h"
#include "Terminal.h"

#include <algorithm>
#include <cstdio>

namespace MaNGOS
{
namespace Console
{

namespace
{
    const char* const SGR_RESET   = "\x1b[0m";
    const char* const SGR_RULE    = "\x1b[38;5;238m";
    const char* const SGR_TITLE   = "\x1b[1;38;5;81m";
    const char* const SGR_SUB     = "\x1b[38;5;244m";
    const char* const SGR_LABEL   = "\x1b[38;5;245m";
    const char* const SGR_VALUE   = "\x1b[1;38;5;252m";
    const char* const SGR_TIME    = "\x1b[38;5;240m";
    const char* const SGR_BARFULL = "\x1b[38;5;73m";
    const char* const SGR_BAREMPT = "\x1b[38;5;237m";
    const char* const SGR_PROMPT  = "\x1b[1;38;5;114m";

    const char* const GLYPH_RULE  = "\xE2\x94\x80";   // U+2500
    const char* const GLYPH_FULL  = "\xE2\x96\x88";   // U+2588
    const char* const GLYPH_EMPTY = "\xE2\x96\x91";   // U+2591
    const char* const GLYPH_DOT   = "\xC2\xB7";       // U+00B7

    const char* StyleSgr(Style s)
    {
        switch (s)
        {
        case STYLE_DETAIL:  return "\x1b[38;5;109m";
        case STYLE_DEBUG:   return "\x1b[38;5;140m";
        case STYLE_SUCCESS: return "\x1b[38;5;114m";
        case STYLE_WARN:    return "\x1b[38;5;179m";
        case STYLE_ERROR:   return "\x1b[1;38;5;203m";
        case STYLE_ACCENT:  return "\x1b[38;5;81m";
        case STYLE_NORMAL:
        default:            return "\x1b[38;5;252m";
        }
    }

    std::size_t Utf8SeqLen(unsigned char lead)
    {
        if (lead < 0x80)          { return 1; }
        if ((lead & 0xE0) == 0xC0) { return 2; }
        if ((lead & 0xF0) == 0xE0) { return 3; }
        if ((lead & 0xF8) == 0xF0) { return 4; }
        return 1;
    }

    void AppendCodePoint(std::string& s, int cp)
    {
        if (cp < 0x80)
        {
            s += char(cp);
        }
        else if (cp < 0x800)
        {
            s += char(0xC0 | (cp >> 6));
            s += char(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            s += char(0xE0 | (cp >> 12));
            s += char(0x80 | ((cp >> 6) & 0x3F));
            s += char(0x80 | (cp & 0x3F));
        }
        else
        {
            s += char(0xF0 | (cp >> 18));
            s += char(0x80 | ((cp >> 12) & 0x3F));
            s += char(0x80 | ((cp >> 6) & 0x3F));
            s += char(0x80 | (cp & 0x3F));
        }
    }

    std::size_t PrevBoundary(const std::string& s, std::size_t pos)
    {
        while (pos > 0)
        {
            --pos;
            if ((static_cast<unsigned char>(s[pos]) & 0xC0) != 0x80)
            {
                break;
            }
        }
        return pos;
    }

    std::size_t NextBoundary(const std::string& s, std::size_t pos)
    {
        if (pos >= s.size())
        {
            return s.size();
        }
        const std::size_t n = Utf8SeqLen(static_cast<unsigned char>(s[pos]));
        return std::min(s.size(), pos + n);
    }

    int CountCodePoints(const std::string& s, std::size_t upto)
    {
        int n = 0;
        for (std::size_t i = 0; i < upto && i < s.size(); i = NextBoundary(s, i))
        {
            ++n;
        }
        return n;
    }

    /// Strip anything that would move the cursor or open an escape sequence.
    std::string Sanitise(const std::string& in)
    {
        std::string out;
        out.reserve(in.size());
        for (std::size_t i = 0; i < in.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(in[i]);
            if (c == '\t')
            {
                out.append(4, ' ');
            }
            else if (c >= 32 && c != 127)
            {
                out += char(c);
            }
        }
        return out;
    }

    /**
     * @brief Fixed-width row assembler.
     *
     * Tracks the visible column while appending, so styling is free (zero
     * width) and text is clipped at the terminal edge instead of wrapping --
     * a wrapped row would desynchronise every row index below it.
     */
    class RowBuilder
    {
        public:
            explicit RowBuilder(int width) : m_col(0), m_max(width)
            {
                m_out.reserve(std::size_t(width) * 2 + 16);
            }

            void Sgr(const char* seq) { m_out += seq; }

            void Text(const std::string& s)
            {
                for (std::size_t i = 0; i < s.size() && m_col < m_max; )
                {
                    const std::size_t n = Utf8SeqLen(static_cast<unsigned char>(s[i]));
                    if (i + n > s.size())
                    {
                        break;
                    }
                    m_out.append(s, i, n);
                    i += n;
                    ++m_col;
                }
            }

            void Glyph(const char* g, int count)
            {
                while (count-- > 0 && m_col < m_max)
                {
                    m_out += g;
                    ++m_col;
                }
            }

            void PadTo(int col)
            {
                while (m_col < col && m_col < m_max)
                {
                    m_out += ' ';
                    ++m_col;
                }
            }

            int Col() const  { return m_col; }
            int Left() const { return m_max - m_col; }

            std::string Take()
            {
                m_out += SGR_RESET;
                return m_out;
            }

        private:
            std::string m_out;
            int         m_col;
            int         m_max;
    };

    std::string MakeRule(int cols)
    {
        RowBuilder b(cols);
        b.Sgr(SGR_RULE);
        b.Glyph(GLYPH_RULE, cols);
        return b.Take();
    }

    /// Length of a leading "HH:MM:SS " stamp, or 0 when the line has none.
    std::size_t TimeStampLen(const std::string& s)
    {
        if (s.size() < 9)
        {
            return 0;
        }
        for (int i = 0; i < 8; ++i)
        {
            const char c = s[std::size_t(i)];
            const bool wantColon = (i == 2 || i == 5);
            if (wantColon ? (c != ':') : (c < '0' || c > '9'))
            {
                return 0;
            }
        }
        return (s[8] == ' ') ? 9 : 0;
    }
}

ConsoleUI::ConsoleUI()
    : m_active(false),
      m_forceRedraw(true),
      m_rows(0),
      m_cols(0),
      m_prompt("mangos> "),
      m_progress(-1),
      m_keyEcho(false),
      m_lastKey(0),
      m_scrollback(DEFAULT_SCROLLBACK),
      m_scroll(0),
      m_cursor(0),
      m_historyPos(-1),
      m_cursorCol(0)
{
}

ConsoleUI& ConsoleUI::Instance()
{
    static ConsoleUI instance;
    return instance;
}

bool ConsoleUI::Start(const std::string& title, const std::string& subtitle)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    if (m_active)
    {
        return true;
    }
    if (!Terminal::Enter())
    {
        return false;
    }

    m_title       = title;
    m_subtitle    = subtitle;
    m_active      = true;
    m_forceRedraw = true;
    return true;
}

void ConsoleUI::Stop()
{
    std::deque<LogLine> tail;
    {
        std::lock_guard<std::mutex> guard(m_mutex);
        if (!m_active)
        {
            return;
        }
        m_active = false;

        const std::size_t keep = std::min<std::size_t>(m_log.size(), 25);
        tail.assign(m_log.end() - std::ptrdiff_t(keep), m_log.end());
    }

    Terminal::Leave();

    // The alternate screen took the session's output with it; replay the tail so
    // the operator is not left staring at a blank shell after a shutdown.
    for (std::deque<LogLine>::const_iterator it = tail.begin(); it != tail.end(); ++it)
    {
        fwrite(it->text.data(), 1, it->text.size(), stdout);
        fputc('\n', stdout);
    }
    fflush(stdout);
}

void ConsoleUI::PushLog(const std::string& text, Style style)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    // One log call is not one row: banners and summaries are written as a single
    // outString() holding a dozen embedded newlines. Splitting here is what keeps
    // them readable -- Sanitise() drops the '\n' itself, so anything not split
    // first collapses into one unreadable row.
    std::string::size_type start = 0;
    for (std::string::size_type nl = text.find('\n');
         nl != std::string::npos;
         nl = text.find('\n', start))
    {
        AppendLog(text.substr(start, nl - start), style);
        start = nl + 1;
    }

    AppendLog(text.substr(start), style);
}

void ConsoleUI::AppendLog(const std::string& text, Style style)
{
    LogLine line;
    line.text  = Sanitise(text);
    line.style = style;
    m_log.push_back(line);

    while (m_log.size() > m_scrollback)
    {
        m_log.pop_front();
    }

    if (m_scroll > 0)
    {
        ++m_scroll;                                     // keep the view pinned while scrolled back
    }
}

void ConsoleUI::PushRaw(const std::string& bytes, Style style)
{
    PushLog(bytes, style);
}

void ConsoleUI::SetStatus(unsigned slot, const std::string& label,
                          const std::string& value, Style style)
{
    if (slot >= MAX_STATUS_FIELDS)
    {
        return;
    }

    std::lock_guard<std::mutex> guard(m_mutex);
    m_status[slot].label = Sanitise(label);
    m_status[slot].value = Sanitise(value);
    m_status[slot].style = style;
    m_status[slot].set   = true;
}

void ConsoleUI::SetActivity(const std::string& text)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_activity = Sanitise(text);
}

void ConsoleUI::SetProgress(int percent)
{
    std::lock_guard<std::mutex> guard(m_mutex);

    // A bar starting is the caller's last log line coming true ("Loading X..."),
    // which is the only label available: loaders announce the work then open a
    // bar that knows nothing about it.
    if (percent == 0 && m_progress < 0 && !m_log.empty())
    {
        const std::string& last = m_log.back().text;
        m_activity = last.substr(TimeStampLen(last));
    }
    else if (percent < 0)
    {
        m_activity.clear();
    }

    m_progress = (percent > 100) ? 100 : percent;
}

void ConsoleUI::SetPrompt(const std::string& prompt)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_prompt = Sanitise(prompt);
}

void ConsoleUI::SetHint(const std::string& hint)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_hint = Sanitise(hint);
}

void ConsoleUI::SetKeyEcho(bool on)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_keyEcho = on;
}

void ConsoleUI::SetScrollback(unsigned lines)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_scrollback = (lines < 100) ? 100 : lines;

    while (m_log.size() > m_scrollback)
    {
        m_log.pop_front();
    }
}

void ConsoleUI::SetHeaderRight(const std::string& text)
{
    std::lock_guard<std::mutex> guard(m_mutex);
    m_headerRight = Sanitise(text);
}

bool ConsoleUI::PollInput(std::string& line)
{
    if (!m_active)
    {
        return false;
    }

    // Drained before taking the lock on purpose: decoding an escape sequence or a
    // multi-byte character polls stdin for up to 25ms per byte, and holding the
    // mutex across that would stall the render thread on every arrow key.
    int keys[64];
    int count = 0;
    while (count < 64)
    {
        const int key = Terminal::ReadKey();
        if (key == KEY_NONE)
        {
            break;
        }
        keys[count++] = key;
    }

    std::lock_guard<std::mutex> guard(m_mutex);

    for (int i = 0; i < count; ++i)
    {
        HandleKey(keys[i]);
    }

    if (m_submitted.empty())
    {
        return false;
    }

    line = m_submitted.front();
    m_submitted.pop_front();
    return true;
}

void ConsoleUI::HandleKey(int key)
{
    m_lastKey = key;

    switch (key)
    {
    case KEY_ENTER:     Submit();                break;
    case KEY_BACKSPACE: EraseBefore();           break;
    case KEY_DELETE:    EraseAt();               break;
    case KEY_LEFT:      MoveLeft();              break;
    case KEY_RIGHT:     MoveRight();             break;
    case KEY_UP:        RecallHistory(1);        break;
    case KEY_DOWN:      RecallHistory(-1);       break;
    case KEY_HOME:      m_cursor = 0;            break;
    case KEY_END:       m_cursor = m_input.size(); break;
    case KEY_RESIZE:    m_forceRedraw = true;    break;

    // Repainting an already-correct frame is invisible, so this also snaps back
    // to the newest line -- otherwise the key looks dead to whoever pressed it.
    case KEY_REDRAW:
        m_forceRedraw = true;
        m_scroll      = 0;
        break;

    case KEY_PAGEUP:
        m_scroll += 10;
        break;

    case KEY_PAGEDOWN:
        m_scroll = (m_scroll > 10) ? m_scroll - 10 : 0;
        break;

    case KEY_KILLLINE:
        m_input.erase(0, m_cursor);
        m_cursor = 0;
        break;

    case KEY_KILLWORD:
        while (m_cursor > 0 && m_input[PrevBoundary(m_input, m_cursor)] == ' ')
        {
            EraseBefore();
        }
        while (m_cursor > 0 && m_input[PrevBoundary(m_input, m_cursor)] != ' ')
        {
            EraseBefore();
        }
        break;

    case KEY_TAB:
    case KEY_EOF:
    case KEY_NONE:
        break;

    default:
        if (key > 0)
        {
            InsertCodePoint(key);
        }
        break;
    }
}

void ConsoleUI::InsertCodePoint(int cp)
{
    std::string encoded;
    AppendCodePoint(encoded, cp);
    m_input.insert(m_cursor, encoded);
    m_cursor += encoded.size();
}

void ConsoleUI::EraseBefore()
{
    if (m_cursor == 0)
    {
        return;
    }
    const std::size_t start = PrevBoundary(m_input, m_cursor);
    m_input.erase(start, m_cursor - start);
    m_cursor = start;
}

void ConsoleUI::EraseAt()
{
    if (m_cursor >= m_input.size())
    {
        return;
    }
    const std::size_t end = NextBoundary(m_input, m_cursor);
    m_input.erase(m_cursor, end - m_cursor);
}

void ConsoleUI::MoveLeft()
{
    m_cursor = PrevBoundary(m_input, m_cursor);
}

void ConsoleUI::MoveRight()
{
    m_cursor = NextBoundary(m_input, m_cursor);
}

void ConsoleUI::RecallHistory(int delta)
{
    if (m_history.empty())
    {
        return;
    }

    int pos = m_historyPos + delta;
    pos = std::max(-1, std::min(pos, int(m_history.size()) - 1));

    m_historyPos = pos;
    m_input      = (pos < 0) ? std::string() : m_history[m_history.size() - 1 - std::size_t(pos)];
    m_cursor     = m_input.size();
}

void ConsoleUI::Submit()
{
    if (!m_input.empty())
    {
        m_history.push_back(m_input);
        m_submitted.push_back(m_input);
        AppendLog(m_prompt + m_input, STYLE_ACCENT);
    }

    m_input.clear();
    m_cursor     = 0;
    m_historyPos = -1;
    m_scroll     = 0;
}

void ConsoleUI::ComposeHeader(std::vector<std::string>& frame, int cols)
{
    RowBuilder b(cols);
    b.PadTo(1);
    b.Sgr(SGR_TITLE);
    b.Text(m_title);

    if (!m_subtitle.empty())
    {
        b.Sgr(SGR_SUB);
        b.Text(" ");
        b.Glyph(GLYPH_DOT, 1);
        b.Text(" ");
        b.Text(m_subtitle);
    }

    if (!m_headerRight.empty())
    {
        const int want = cols - 1 - CountCodePoints(m_headerRight, m_headerRight.size());
        if (want > b.Col())
        {
            b.PadTo(want);
            b.Sgr(SGR_SUB);
            b.Text(m_headerRight);
        }
    }

    frame.push_back(b.Take());
    frame.push_back(MakeRule(cols));
}

void ConsoleUI::ComposeStatus(std::vector<std::string>& frame, int cols)
{
    const int fieldWidth = 22;

    std::vector<const StatusField*> fields;
    for (unsigned i = 0; i < MAX_STATUS_FIELDS; ++i)
    {
        if (m_status[i].set)
        {
            fields.push_back(&m_status[i]);
        }
    }

    if (!fields.empty())
    {
        RowBuilder b(cols);

        for (std::size_t i = 0; i < fields.size(); ++i)
        {
            b.PadTo(2 + int(i) * fieldWidth);
            b.Sgr(SGR_LABEL);
            b.Text(fields[i]->label);
            b.Text("  ");
            b.Sgr(fields[i]->style == STYLE_NORMAL ? SGR_VALUE
                                                   : StyleSgr(fields[i]->style));
            b.Text(fields[i]->value);
        }

        frame.push_back(b.Take());
    }

    if (m_progress >= 0 || !m_activity.empty())
    {
        const int barWidth = std::max(10, std::min(32, cols / 3));
        RowBuilder b(cols);
        b.PadTo(1);
        b.Sgr(StyleSgr(STYLE_ACCENT));
        b.Text(m_activity);

        if (m_progress >= 0)
        {
            const int barStart = cols - barWidth - 7;
            if (barStart > b.Col() + 1)
            {
                b.PadTo(barStart);
                const int filled = m_progress * barWidth / 100;
                b.Sgr(SGR_BARFULL);
                b.Glyph(GLYPH_FULL, filled);
                b.Sgr(SGR_BAREMPT);
                b.Glyph(GLYPH_EMPTY, barWidth - filled);
                b.Sgr(SGR_VALUE);
                char pct[8];
                snprintf(pct, sizeof(pct), " %3d%%", m_progress);
                b.Text(pct);
            }
        }

        frame.push_back(b.Take());
    }

    frame.push_back(MakeRule(cols));
}

void ConsoleUI::ComposeLog(std::vector<std::string>& frame, int cols, int height)
{
    if (height <= 0)
    {
        return;
    }

    const int total   = int(m_log.size());
    const int maxBack = std::max(0, total - height);
    if (m_scroll > maxBack)
    {
        m_scroll = maxBack;
    }

    const int last  = total - m_scroll;
    const int first = std::max(0, last - height);

    for (int i = first; i < last; ++i)
    {
        const LogLine& line = m_log[std::size_t(i)];
        RowBuilder b(cols);
        b.PadTo(1);

        const std::size_t stamp = TimeStampLen(line.text);
        if (stamp)
        {
            b.Sgr(SGR_TIME);
            b.Text(line.text.substr(0, stamp));
        }

        b.Sgr(StyleSgr(line.style));
        b.Text(line.text.substr(stamp));
        frame.push_back(b.Take());
    }

    for (int i = last - first; i < height; ++i)
    {
        frame.push_back(std::string(SGR_RESET));
    }
}

void ConsoleUI::ComposeInput(std::vector<std::string>& frame, int cols)
{
    RowBuilder b(cols);
    b.PadTo(1);
    b.Sgr(SGR_PROMPT);
    b.Text(m_prompt);

    m_cursorCol = std::min(cols - 1, b.Col() + CountCodePoints(m_input, m_cursor));

    b.Sgr(SGR_RESET);
    b.Text(m_input);

    std::string right = m_hint;
    if (m_scroll > 0)
    {
        char note[48];
        snprintf(note, sizeof(note), "scrolled back %d line(s)", m_scroll);
        right = note;
    }
    if (m_keyEcho)
    {
        char note[48];
        snprintf(note, sizeof(note), "key %d  scroll %d  lines %d",
                 m_lastKey, m_scroll, int(m_log.size()));
        right = note;
    }

    if (!right.empty())
    {
        const int want = cols - 1 - CountCodePoints(right, right.size());
        if (want > b.Col() + 2)
        {
            b.PadTo(want);
            b.Sgr(SGR_TIME);
            b.Text(right);
        }
    }

    frame.push_back(b.Take());
}

void ConsoleUI::ComposeFrame(std::vector<std::string>& frame, int rows, int cols)
{
    frame.clear();
    frame.reserve(std::size_t(rows));

    ComposeHeader(frame, cols);
    ComposeStatus(frame, cols);

    const int chrome    = int(frame.size()) + 2;
    const int logHeight = std::max(0, rows - chrome);

    ComposeLog(frame, cols, logHeight);
    frame.push_back(MakeRule(cols));
    ComposeInput(frame, cols);

    frame.resize(std::size_t(rows), std::string(SGR_RESET));
}

int ConsoleUI::Columns() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cols;
}

void ConsoleUI::Render()
{
    if (!m_active)
    {
        return;
    }

    int rows = 0;
    int cols = 0;
    Terminal::Size(rows, cols);

    std::string out;
    {
        std::lock_guard<std::mutex> guard(m_mutex);

        // Re-checked under the lock: Stop() may have restored the terminal
        // between the early-out above and here, and this would then paint
        // escape sequences onto the operator's shell.
        if (!m_active)
        {
            return;
        }

        if (rows != m_rows || cols != m_cols)
        {
            m_rows        = rows;
            m_cols        = cols;
            m_forceRedraw = true;
            m_frame.clear();
        }

        if (rows < 8 || cols < 40)
        {
            return;
        }

        std::vector<std::string> frame;
        ComposeFrame(frame, rows, cols);

        if (m_forceRedraw)
        {
            out += "\x1b[2J";
        }

        for (std::size_t r = 0; r < frame.size(); ++r)
        {
            if (!m_forceRedraw && r < m_frame.size() && m_frame[r] == frame[r])
            {
                continue;
            }
            out += "\x1b[";
            out += std::to_string(r + 1);
            out += ";1H";
            out += frame[r];
            out += "\x1b[K";
        }

        m_frame.swap(frame);
        m_forceRedraw = false;

        out += "\x1b[";
        out += std::to_string(rows);
        out += ";";
        out += std::to_string(m_cursorCol + 1);
        out += "H";
    }

    // Hidden across the repaint so the caret does not streak over every row being
    // painted, then shown where it was parked: the terminal's own cursor, which
    // blinks and matches what the operator expects.
    Terminal::Write("\x1b[?25l" + out + "\x1b[?25h");
}

}
}
