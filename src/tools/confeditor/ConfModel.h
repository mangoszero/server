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

#ifndef MANGOS_CONFEDITOR_CONFMODEL_H
#define MANGOS_CONFEDITOR_CONFMODEL_H

#include <string>
#include <vector>

namespace conf
{
    /// What the editor should put in front of the value.
    enum Kind
    {
        KIND_TEXT,      ///< free string
        KIND_NUMBER,    ///< integer or float
        KIND_BOOL,      ///< 1/0 shown as a checkbox
        KIND_CHOICE,    ///< a fixed set of quoted words, shown as a dropdown
        KIND_ENUM,      ///< numbers the doc gives names to, shown as a dropdown
        KIND_PATH,      ///< string that names a directory, gets a Browse button
        KIND_CONNECTION ///< host;port;user;password;database, shown as five fields
    };

    struct Entry
    {
        std::string key;
        std::string value;      ///< as written, minus any surrounding quotes
        std::string doc;        ///< the '#' block above, comment markers stripped
        std::vector<std::string> choices;
        std::vector<std::string> optionValues;  ///< KIND_ENUM: the numbers
        std::vector<std::string> optionLabels;  ///< KIND_ENUM: what they mean
        size_t      line = 0;   ///< index into ConfFile::lines
        size_t      section = 0;
        size_t      group = 0;  ///< tab; may differ from section (Rate.* → Rates)
        Kind        kind = KIND_TEXT;
        bool        quoted = false;
    };

    struct Section
    {
        std::string title;      ///< banner text, e.g. "PERFORMANCE SETTINGS"
        size_t      group = 0;  ///< index into ConfFile::Groups()
    };

    /**
     * @brief A mangosd.conf held so it can be written back unchanged.
     *
     * Every line of the file is kept verbatim. Saving rewrites only the value of
     * the assignments the user actually touched, so comments, ordering, spacing
     * and the file's CRLF endings survive a round trip -- an editor that
     * reformats a config it did not fully understand is how hand-tuned settings
     * get silently dropped.
     */
    class ConfFile
    {
        public:

            bool Load(const std::string& path, std::string& error);
            bool Save(const std::string& path, std::string& error) const;

            const std::string& Path() const { return m_path; }

            const std::vector<Section>& Sections() const { return m_sections; }
            const std::vector<Entry>& Entries() const { return m_entries; }

            /// Tab names. Several banner sections share one; see GroupFor.
            const std::vector<std::string>& Groups() const { return m_groups; }

            /// Overwrite one entry's value; returns false if unchanged.
            bool SetValue(size_t index, const std::string& value);

            bool Dirty() const { return m_dirty; }
            void ClearDirty() { m_dirty = false; }

        private:

            void Classify(Entry& entry) const;

            std::string              m_path;
            std::vector<std::string> m_lines;   ///< without line terminators
            std::vector<std::string> m_endings; ///< the terminator each line had
            std::vector<Section>     m_sections;
            std::vector<std::string> m_groups;
            std::vector<Entry>       m_entries;
            bool                     m_dirty = false;
    };

    /// "Rate.XP.PetKill" -> "Rate . XP . Pet kill", for a label a human reads.
    std::string Humanize(const std::string& key);
}

#endif
