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

#ifndef MANGOS_H_SPELLCOOLDOWNMGR
#define MANGOS_H_SPELLCOOLDOWNMGR

#include "Common.h"
#include <map>

class Player;
class Spell;
class QueryResult;
struct SpellEntry;

/**
 * @brief Structure to hold spell cooldown information
 */
struct SpellCooldown
{
    time_t end;    ///< End time of the cooldown
    uint16 itemid; ///< Item ID associated with the cooldown
};

typedef std::map<uint32, SpellCooldown> SpellCooldowns;

/**
 * @brief Owns a player's active spell-cooldown map and the operations on it.
 *
 * Held by value on Player as m_spellCooldownMgr with a Player* back-pointer.
 * Persisted to character_spell_cooldown via LoadFromDB()/SaveToDB().
 */
class SpellCooldownMgr
{
    public:
        explicit SpellCooldownMgr(Player* owner) : m_owner(owner) {}

        SpellCooldowns const& GetSpellCooldownMap() const { return m_cooldowns; }

        bool HasSpellCooldown(uint32 spell_id) const
        {
            SpellCooldowns::const_iterator itr = m_cooldowns.find(spell_id);
            return itr != m_cooldowns.end() && itr->second.end > time(NULL);
        }

        time_t GetSpellCooldownDelay(uint32 spell_id) const
        {
            SpellCooldowns::const_iterator itr = m_cooldowns.find(spell_id);
            time_t t = time(NULL);
            return itr != m_cooldowns.end() && itr->second.end > t ? itr->second.end - t : 0;
        }

        void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* spell = NULL, bool infinityCooldown = false);
        void AddSpellCooldown(uint32 spell_id, uint32 itemid, time_t end_time);
        void SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId = 0, Spell* spell = NULL);
        void RemoveSpellCooldown(uint32 spell_id, bool update = false);
        void RemoveSpellCategoryCooldown(uint32 cat, bool update = false);
        void RemoveAllSpellCooldown();
        void LoadFromDB(QueryResult* result);
        void SaveToDB();

    private:
        Player* m_owner;            ///< Non-owning pointer to the owning Player.
        SpellCooldowns m_cooldowns; ///< Active spell cooldowns keyed by spell id.
};

#endif // MANGOS_H_SPELLCOOLDOWNMGR
