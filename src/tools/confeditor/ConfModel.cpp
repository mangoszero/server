/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

// The conf file is the source of truth for the UI, including the help text: every
// key is already documented in a '#    Key' block above the assignments, so the
// editor reads those instead of carrying its own copy. A description here could
// go stale against the server; one read out of the file it is editing cannot.

#include "ConfModel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace
{
    const char* const WS = " \t";

    std::string Trim(const std::string& s)
    {
        const size_t a = s.find_first_not_of(WS);
        if (a == std::string::npos)
        {
            return std::string();
        }
        const size_t b = s.find_last_not_of(WS);
        return s.substr(a, b - a + 1);
    }

    bool IsRule(const std::string& s)
    {
        return s.size() >= 10 && s.find_first_not_of('#') == std::string::npos;
    }

    /// A doc entry header: '#' then 2+ spaces then a bare key and nothing else.
    bool DocHeader(const std::string& line, std::string& key)
    {
        if (line.empty() || line[0] != '#')
        {
            return false;
        }

        size_t i = 1;
        size_t spaces = 0;
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        {
            ++i;
            ++spaces;
        }
        if (spaces < 2 || i >= line.size())
        {
            return false;
        }

        const size_t start = i;
        while (i < line.size() &&
               (std::isalnum((unsigned char)line[i]) || line[i] == '_' || line[i] == '.'))
        {
            ++i;
        }
        if (i == start)
        {
            return false;
        }

        std::string rest = Trim(line.substr(i));
        if (rest == ":")
        {
            rest.clear();
        }
        if (!rest.empty() && rest[0] != '#')
        {
            return false;
        }

        key = line.substr(start, i - start);
        return true;
    }

    bool Assignment(const std::string& line, std::string& key, std::string& value)
    {
        const std::string t = Trim(line);
        if (t.empty() || t[0] == '#' || t[0] == '[' || t[0] == ';')
        {
            return false;
        }

        const size_t eq = t.find('=');
        if (eq == std::string::npos)
        {
            return false;
        }

        key = Trim(t.substr(0, eq));
        value = Trim(t.substr(eq + 1));
        return !key.empty();
    }

    std::string Unquote(const std::string& s, bool& quoted)
    {
        quoted = s.size() >= 2 && s.front() == '"' && s.back() == '"';
        return quoted ? s.substr(1, s.size() - 2) : s;
    }

    bool Numeric(const std::string& s)
    {
        if (s.empty())
        {
            return false;
        }
        char* end = nullptr;
        std::strtod(s.c_str(), &end);
        return end && *end == '\0';
    }

    std::string Lower(std::string s)
    {
        for (char& c : s)
        {
            c = (char)std::tolower((unsigned char)c);
        }
        return s;
    }

    bool WordChar(unsigned char c)
    {
        return std::isalnum(c) != 0 || c == '_' || c == '-';
    }

    bool PlausibleChoice(const std::string& word)
    {
        if (word.size() < 2 || word.size() > 24)
        {
            return false;
        }
        // Prose leftovers from "enable or disable the ..." style lines.
        static const char* const SKIP[] =
        {
            "the", "for", "and", "not", "any", "all", "each", "with", "from",
            "this", "that", "when", "will", "can", "may", "use", "set", "only",
            "both", "are", "was", "has", "have", "been", "also", "into", "over"
        };
        for (const char* s : SKIP)
        {
            if (Lower(word) == s)
            {
                return false;
            }
        }
        return true;
    }

    void AddChoice(std::vector<std::string>& out, const std::string& word)
    {
        if (!PlausibleChoice(word))
        {
            return;
        }
        if (std::find(out.begin(), out.end(), word) == out.end())
        {
            out.push_back(word);
        }
    }

    /// Quoted words and bare "a or b" pairs, e.g. "auto"/"plain", shutdown or restart.
    void CollectChoices(const std::string& doc, std::vector<std::string>& out)
    {
        size_t i = 0;
        while ((i = doc.find('"', i)) != std::string::npos)
        {
            const size_t end = doc.find('"', i + 1);
            if (end == std::string::npos)
            {
                break;
            }

            const std::string word = doc.substr(i + 1, end - i - 1);
            i = end + 1;

            if (word.find_first_of(" \t;/\\.") != std::string::npos)
            {
                continue;
            }
            AddChoice(out, word);
        }

        // Unquoted alternatives written as their own line, e.g. `shutdown or restart.`
        // Mid-sentence prose ("relative or absolute", "file or a pipe") is ignored.
        const std::string lower = Lower(doc);
        size_t at = 0;
        while ((at = lower.find(" or ", at)) != std::string::npos)
        {
            size_t leftEnd = at;
            while (leftEnd > 0 && (doc[leftEnd - 1] == ' ' || doc[leftEnd - 1] == '\t'))
            {
                --leftEnd;
            }
            size_t leftStart = leftEnd;
            while (leftStart > 0 && WordChar((unsigned char)doc[leftStart - 1]))
            {
                --leftStart;
            }

            size_t rightStart = at + 4;
            while (rightStart < doc.size() &&
                   (doc[rightStart] == ' ' || doc[rightStart] == '\t'))
            {
                ++rightStart;
            }
            size_t rightEnd = rightStart;
            while (rightEnd < doc.size() && WordChar((unsigned char)doc[rightEnd]))
            {
                ++rightEnd;
            }

            if (leftStart >= leftEnd || rightStart >= rightEnd)
            {
                at += 4;
                continue;
            }

            size_t lineStart = doc.rfind('\n', leftStart);
            lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
            size_t lineEnd = doc.find('\n', rightEnd);
            if (lineEnd == std::string::npos)
            {
                lineEnd = doc.size();
            }

            std::string line = doc.substr(lineStart, lineEnd - lineStart);
            while (!line.empty() &&
                   (line.back() == '\r' || line.back() == '\n' || line.back() == ' ' ||
                    line.back() == '\t' || line.back() == '.' || line.back() == ';'))
            {
                line.pop_back();
            }
            while (!line.empty() &&
                   (line.front() == ' ' || line.front() == '\t'))
            {
                line.erase(line.begin());
            }

            const std::string left = doc.substr(leftStart, leftEnd - leftStart);
            const std::string right = doc.substr(rightStart, rightEnd - rightStart);
            if (line == left + " or " + right)
            {
                AddChoice(out, left);
                AddChoice(out, right);
            }
            at += 4;
        }
    }

    /**
     * @brief The values a doc block offers, as in "0 - off" or "1 (enabled)".
     *
     * Structure rather than wording, because the conf says the same thing a
     * dozen ways. A block that names only 0 and 1 documents a two-state switch;
     * one that names a 2 documents a mode, and a checkbox could not express it.
     */
    void DocumentedValues(const std::string& doc, bool& only01, bool& any)
    {
        only01 = true;
        any = false;

        for (size_t i = 0; i < doc.size(); ++i)
        {
            if (!std::isdigit((unsigned char)doc[i]))
            {
                continue;
            }
            if (i && (std::isalnum((unsigned char)doc[i - 1]) || doc[i - 1] == '.'))
            {
                continue;
            }

            size_t j = i;
            while (j < doc.size() && std::isdigit((unsigned char)doc[j]))
            {
                ++j;
            }
            const std::string number = doc.substr(i, j - i);

            size_t k = j;
            while (k < doc.size() && (doc[k] == ' ' || doc[k] == '\t'))
            {
                ++k;
            }
            const bool introduces = k < doc.size() &&
                                    (doc[k] == '(' || doc[k] == '-' || doc[k] == '=');
            i = j - 1;

            if (!introduces || number.size() > 4)
            {
                continue;
            }

            any = true;
            if (number != "0" && number != "1")
            {
                only01 = false;
            }
        }
    }

    /**
     * @brief Pull "0 = NORMAL; 1 = PVP" style tables out of a doc block.
     *
     * These are the settings whose numbers mean something -- GameType, the DBC
     * locale, the character-deletion method. The conf already spells out what
     * each number is, so the editor can offer the names instead of asking an
     * admin to remember that a realm is 1 and not 6.
     */
    void CollectOptions(const std::string& doc,
                        std::vector<std::string>& values,
                        std::vector<std::string>& labels)
    {
        size_t i = 0;
        while (i < doc.size())
        {
            if (!std::isdigit((unsigned char)doc[i]) ||
                (i && (std::isalnum((unsigned char)doc[i - 1]) || doc[i - 1] == '.')))
            {
                ++i;
                continue;
            }

            size_t j = i;
            while (j < doc.size() && std::isdigit((unsigned char)doc[j]))
            {
                ++j;
            }
            const std::string number = doc.substr(i, j - i);

            size_t k = j;
            while (k < doc.size() && (doc[k] == ' ' || doc[k] == '\t'))
            {
                ++k;
            }
            if (k >= doc.size() || (doc[k] != '=' && doc[k] != '-'))
            {
                i = j;
                continue;
            }

            ++k;
            while (k < doc.size() && (doc[k] == ' ' || doc[k] == '\t'))
            {
                ++k;
            }

            size_t end = k;
            while (end < doc.size() && doc[end] != ';' && doc[end] != '\r' &&
                   doc[end] != '\n')
            {
                ++end;
            }

            const std::string label = Trim(doc.substr(k, end - k));
            i = end;

            if (label.empty() || label.size() > 48 || number.size() > 5 ||
                !std::isalpha((unsigned char)label[0]))
            {
                continue;
            }
            if (std::find(values.begin(), values.end(), number) != values.end())
            {
                continue;
            }

            values.push_back(number);
            labels.push_back(label);
        }
    }

    bool MentionsSwitch(const std::string& doc)
    {
        static const char* const WORDS[] =
        {
            "enable", "disable", "allow", "true", "false", "yes)", "no)",
            " on ", " off ", "send", "show", "use", "active", "turn"
        };

        for (const char* w : WORDS)
        {
            if (doc.find(w) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Which tab a banner section belongs under.
     *
     * A conf carries about twenty banners, which is more tabs than anyone wants
     * to read across; several of them are one subject split by topic. The order
     * below is the matching order and it matters -- "SERVER LOGGING" and "SERVER
     * RATES" both contain SERVER, so the specific words are tested first.
     */
    const char* GroupFor(const std::string& title)
    {
        static const struct { const char* needle; const char* group; } MAP[] =
        {
            { "LOG",          "Logging"  },
            { "RATE",         "Rates"    },
            { "BATTLEGROUND", "PvP"      },
            { "PVP",          "PvP"      },
            { "ARENA",        "PvP"      },
            { "HONOR",        "PvP"      },
            { "NETWORK",      "Console"  },
            { "CONSOLE",      "Console"  },
            { "SOAP",         "Console"  },
            { "REMOTE",       "Console"  },
            { "WARDEN",       "Console"  },
            { "ELUNA",        "Modules"  },
            { "SCRIPT",       "Modules"  },
            { "AUCTION",      "Modules"  },
            { "BOT",          "Modules"  },
            { "MODULE",       "Modules"  },
            { "CONNECTION",   "Server"   },
            { "DIRECTOR",     "Server"   },
            { "PERFORMANCE",  "Server"   },
            { "DATABASE",     "Server"   },
            { "REALM",        "Server"   },
            { "PLAYER",       "Players"  },
            { "CHAT",         "Players"  },
            { "GAME MASTER",  "Players"  },
            { "SOCIAL",       "Players"  },
            { "CREATURE",     "World"    },
            { "GAMEOBJECT",   "World"    },
            { "LIVINGWORLD",  "World"    },
            { "VISIBILITY",   "World"    },
            { "CINEMATIC",    "World"    },
            { "QUEST",        "World"    },
            { "SERVER",       "World"    },
            { "WORLD",        "World"    },
        };

        for (const auto& m : MAP)
        {
            if (title.find(m.needle) != std::string::npos)
            {
                return m.group;
            }
        }
        return "Other";
    }

    /// Tabs appear in this order whatever order the banners came in.
    const char* const GROUP_ORDER[] =
    {
        "Server", "World", "Players", "Rates", "PvP", "Console", "Logging",
        "Modules", "Other"
    };
}

namespace conf
{

bool ConfFile::Load(const std::string& path, std::string& error)
{
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in.is_open())
    {
        error = "Cannot open " + path;
        return false;
    }

    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string text = buf.str();

    m_path = path;
    m_lines.clear();
    m_endings.clear();
    m_sections.clear();
    m_entries.clear();
    m_dirty = false;

    size_t i = 0;
    while (i <= text.size())
    {
        const size_t nl = text.find('\n', i);
        if (nl == std::string::npos)
        {
            if (i < text.size())
            {
                m_lines.push_back(text.substr(i));
                m_endings.push_back(std::string());
            }
            break;
        }

        std::string line = text.substr(i, nl - i);
        std::string ending = "\n";
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
            ending = "\r\n";
        }
        m_lines.push_back(line);
        m_endings.push_back(ending);
        i = nl + 1;
    }

    for (const char* g : GROUP_ORDER)
    {
        m_groups.push_back(g);
    }

    auto groupIndex = [&](const std::string& title) -> size_t
    {
        const std::string name = GroupFor(title);
        for (size_t i = 0; i < m_groups.size(); ++i)
        {
            if (m_groups[i] == name)
            {
                return i;
            }
        }
        return m_groups.size() - 1;
    };

    Section general;
    general.title = "General";
    general.group = groupIndex("CONNECTIONS");
    m_sections.push_back(general);

    std::map<std::string, std::string> docs;
    std::vector<std::string> pending;
    std::string block;

    auto flush = [&]()
    {
        const std::string body = Trim(block);
        for (const std::string& k : pending)
        {
            if (!body.empty())
            {
                docs[Lower(k)] = body;
            }
        }
        pending.clear();
        block.clear();
    };

    for (size_t n = 0; n < m_lines.size(); ++n)
    {
        const std::string& line = m_lines[n];

        if (IsRule(line))
        {
            flush();
            // A banner is the rule, a '# TITLE' line, then the key docs. Only a
            // line that is all caps is a title; anything else is prose.
            if (n + 1 < m_lines.size() && !m_lines[n + 1].empty() && m_lines[n + 1][0] == '#')
            {
                const std::string candidate = Trim(m_lines[n + 1].substr(1));
                const bool caps = !candidate.empty() &&
                                  candidate.find_first_of("abcdefghijklmnopqrstuvwxyz") == std::string::npos;
                if (caps && candidate.size() > 2)
                {
                    Section s;
                    s.title = candidate;
                    s.group = groupIndex(candidate);
                    m_sections.push_back(s);
                }
            }
            continue;
        }

        std::string key;
        std::string value;

        if (DocHeader(line, key))
        {
            // Consecutive headers share the description that follows them.
            if (!block.empty())
            {
                flush();
            }
            pending.push_back(key);
            continue;
        }

        if (!line.empty() && line[0] == '#')
        {
            if (!pending.empty())
            {
                std::string body = line.substr(1);
                const size_t a = body.find_first_not_of(WS);
                body = (a == std::string::npos) ? std::string() : body.substr(a);
                if (body.empty())
                {
                    flush();
                }
                else
                {
                    block += body;
                    block += "\r\n";
                }
            }
            continue;
        }

        if (Assignment(line, key, value))
        {
            flush();

            Entry e;
            e.key = key;
            e.value = Unquote(value, e.quoted);
            e.line = n;
            e.section = m_sections.size() - 1;
            e.group = m_sections[e.section].group;

            // Rate.* keys often sit under CREATURE / WORLD banners in the file;
            // the Rates tab is where admins look for them.
            const std::string keyLower = Lower(key);
            if (keyLower.size() >= 5 && keyLower.compare(0, 5, "rate.") == 0)
            {
                e.group = groupIndex("SERVER RATES");
            }

            const auto it = docs.find(keyLower);
            if (it != docs.end())
            {
                e.doc = it->second;
            }

            Classify(e);
            m_entries.push_back(e);
        }
    }

    if (m_entries.empty())
    {
        error = path + " holds no configuration keys.";
        return false;
    }

    return true;
}

/// Last segment of a key: "Eluna.ScriptPath" -> "scriptpath".
std::string KeyLeaf(const std::string& key)
{
    const size_t dot = key.rfind('.');
    return (dot == std::string::npos) ? key : key.substr(dot + 1);
}

bool PathLeaf(const std::string& key)
{
    const std::string leaf = KeyLeaf(key);
    static const char* const TAILS[] = { "dir", "path", "directory", "folder" };
    for (const char* t : TAILS)
    {
        const size_t n = std::strlen(t);
        if (leaf.size() >= n && leaf.compare(leaf.size() - n, n, t) == 0)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Decide which control the value gets.
 *
 * Read off the documented defaults rather than guessed from the key name: the
 * doc block is the only place that says a 0/1 is a switch and not a count.
 */
void ConfFile::Classify(Entry& entry) const
{
    const std::string doc = Lower(entry.doc);
    const std::string key = Lower(entry.key);

    bool only01 = false;
    bool documented = false;
    DocumentedValues(doc, only01, documented);

    // Rate.* and percent scales (0 / 1 / 1.5) document 0 as "off", which is not a switch.
    const bool rateScale =
        (key.size() >= 5 && key.compare(0, 5, "rate.") == 0) ||
        doc.find('%') != std::string::npos ||
        doc.find("percent") != std::string::npos ||
        doc.find("1.5") != std::string::npos;

    if ((entry.value == "0" || entry.value == "1") &&
        documented && only01 && MentionsSwitch(doc) && !rateScale)
    {
        entry.kind = KIND_BOOL;
        return;
    }

    if (entry.value == "true" || entry.value == "false")
    {
        entry.kind = KIND_BOOL;
        return;
    }

    // host;port;user;password;database -- five fields pretending to be a string.
    if (entry.quoted && key.find("database") != std::string::npos &&
        std::count(entry.value.begin(), entry.value.end(), ';') == 4)
    {
        entry.kind = KIND_CONNECTION;
        return;
    }

    // DataDir, LogsDir, Eluna.ScriptPath, AH.Service.Path -- never a connection string.
    if (entry.value.find(';') == std::string::npos && PathLeaf(key))
    {
        entry.kind = KIND_PATH;
        return;
    }

    if (Numeric(entry.value))
    {
        CollectOptions(entry.doc, entry.optionValues, entry.optionLabels);

        const bool listed = std::find(entry.optionValues.begin(),
                                      entry.optionValues.end(),
                                      entry.value) != entry.optionValues.end();
        if (entry.optionValues.size() >= 2 && listed)
        {
            entry.kind = KIND_ENUM;
            return;
        }
        entry.optionValues.clear();
        entry.optionLabels.clear();
    }

    if (entry.quoted)
    {
        CollectChoices(entry.doc, entry.choices);

        const bool listed = std::find(entry.choices.begin(), entry.choices.end(),
                                      entry.value) != entry.choices.end();
        if (entry.choices.size() >= 2 && listed)
        {
            entry.kind = KIND_CHOICE;
            return;
        }
        entry.choices.clear();

        entry.kind = KIND_TEXT;
        return;
    }

    entry.kind = Numeric(entry.value) ? KIND_NUMBER : KIND_TEXT;
}

bool ConfFile::SetValue(size_t index, const std::string& value)
{
    if (index >= m_entries.size() || m_entries[index].value == value)
    {
        return false;
    }

    m_entries[index].value = value;
    m_dirty = true;
    return true;
}

bool ConfFile::Save(const std::string& path, std::string& error) const
{
    std::vector<std::string> out = m_lines;

    for (const Entry& e : m_entries)
    {
        const std::string& original = m_lines[e.line];
        const size_t eq = original.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        // Keep everything up to and including the gap after '=' byte for byte, so
        // neither the column alignment nor a line written without a space around
        // the '=' is quietly re-flowed by the editor.
        size_t start = eq + 1;
        while (start < original.size() && (original[start] == ' ' || original[start] == '\t'))
        {
            ++start;
        }

        std::string text = e.quoted ? "\"" + e.value + "\"" : e.value;
        out[e.line] = original.substr(0, start) + text;
    }

    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f.is_open())
    {
        error = "Cannot write " + path;
        return false;
    }

    for (size_t i = 0; i < out.size(); ++i)
    {
        f << out[i] << m_endings[i];
    }

    if (!f.good())
    {
        error = "Failed while writing " + path;
        return false;
    }
    return true;
}

std::string Humanize(const std::string& key)
{
    static const char* const ACRONYM[] =
    {
        "XP", "IP", "ID", "RA", "GM", "BG", "AH", "PvP", "SOAP", "DBC", "MB",
        "TCP", "UDP", "URL", "HTTP", "SQL", "CPU", "UI", "NPC", "AI", "MOTD",
        "SD3", "PID", "DB", "MMAP", "VMAP", "LOS", "AoE"
    };

    std::string out;
    size_t start = 0;

    while (start <= key.size())
    {
        const size_t dot = key.find('.', start);
        const std::string part = key.substr(start, (dot == std::string::npos)
                                                   ? std::string::npos : dot - start);

        std::vector<std::string> words;
        std::string current;
        for (size_t i = 0; i < part.size(); ++i)
        {
            const char c = part[i];
            const bool upper = std::isupper((unsigned char)c) != 0;
            const bool boundary =
                upper && !current.empty() &&
                (!std::isupper((unsigned char)current.back()) ||
                 (i + 1 < part.size() && std::islower((unsigned char)part[i + 1])));

            if (boundary)
            {
                words.push_back(current);
                current.clear();
            }
            current += c;
        }
        if (!current.empty())
        {
            words.push_back(current);
        }

        std::string text;
        for (std::string& w : words)
        {
            bool acronym = false;
            for (const char* a : ACRONYM)
            {
                if (Lower(w) == Lower(a))
                {
                    w = a;
                    acronym = true;
                    break;
                }
            }
            if (!acronym)
            {
                w = Lower(w);
            }
            if (!text.empty())
            {
                text += ' ';
            }
            text += w;
        }

        if (!text.empty() && std::islower((unsigned char)text[0]))
        {
            text[0] = (char)std::toupper((unsigned char)text[0]);
        }

        if (!out.empty())
        {
            // ASCII, and the caller swaps in whatever it wants to draw. Anything
            // else here would be bytes in one encoding decoded in another the
            // moment this string reaches a window.
            out += " - ";
        }
        out += text;

        if (dot == std::string::npos)
        {
            break;
        }
        start = dot + 1;
    }

    return out;
}

}
