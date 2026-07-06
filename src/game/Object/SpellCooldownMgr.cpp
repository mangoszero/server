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

#include "SpellCooldownMgr.h"
#include "Player.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "Spell.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"

void SpellCooldownMgr::AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* spell, bool infinityCooldown)
{
    // init cooldown values
    uint32 cat   = 0;
    int32 rec    = -1;
    int32 catrec = -1;

    // some special item spells without correct cooldown in SpellInfo
    // cooldown information stored in item prototype
    // This used in same way in WorldSession::HandleItemQuerySingleOpcode data sending to client.

    if (itemId)
    {
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemId))
        {
            for (int idx = 0; idx < MAX_ITEM_PROTO_SPELLS; ++idx)
            {
                if (proto->Spells[idx].SpellId == spellInfo->Id)
                {
                    cat    = proto->Spells[idx].SpellCategory;
                    rec    = proto->Spells[idx].SpellCooldown;
                    catrec = proto->Spells[idx].SpellCategoryCooldown;
                    break;
                }
            }
        }
    }

    // if no cooldown found above then base at DBC data
    if (rec < 0 && catrec < 0)
    {
        cat = spellInfo->Category;
        rec = spellInfo->RecoveryTime;
        catrec = spellInfo->CategoryRecoveryTime;
    }

    time_t curTime = time(NULL);

    time_t catrecTime;
    time_t recTime;

    // overwrite time for selected category
    if (infinityCooldown)
    {
        // use +MONTH as infinity mark for spell cooldown (will checked as MONTH/2 at save ans skipped)
        // but not allow ignore until reset or re-login
        catrecTime = catrec > 0 ? curTime + Player::infinityCooldownDelay : 0;
        recTime    = rec    > 0 ? curTime + Player::infinityCooldownDelay : catrecTime;
    }
    else
    {
        // shoot spells used equipped item cooldown values already assigned in GetAttackTime(RANGED_ATTACK)
        // prevent 0 cooldowns set by another way
        if (rec <= 0 && catrec <= 0 && (cat == 76 || cat == 351))
        {
            rec = m_owner->GetAttackTime(RANGED_ATTACK);
        }

        // Now we have cooldown data (if found any), time to apply mods
        if (rec > 0)
        {
            m_owner->ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, rec, spell);
        }

        if (catrec > 0)
        {
            m_owner->ApplySpellMod(spellInfo->Id, SPELLMOD_COOLDOWN, catrec, spell);
        }

        // replace negative cooldowns by 0
        if (rec < 0)
        {
            rec = 0;
        }
        if (catrec < 0)
        {
            catrec = 0;
        }

        // no cooldown after applying spell mods
        if (rec == 0 && catrec == 0)
        {
            return;
        }

        catrecTime = catrec ? curTime + catrec / IN_MILLISECONDS : 0;
        recTime    = rec ? curTime + rec / IN_MILLISECONDS : catrecTime;
    }

    // self spell cooldown
    if (recTime > 0)
    {
        AddSpellCooldown(spellInfo->Id, itemId, recTime);
    }

    // category spells
    if (cat && catrec > 0)
    {
        SpellCategoryStore::const_iterator i_scstore = sSpellCategoryStore.find(cat);
        if (i_scstore != sSpellCategoryStore.end())
        {
            for (SpellCategorySet::const_iterator i_scset = i_scstore->second.begin(); i_scset != i_scstore->second.end(); ++i_scset)
            {
                if (*i_scset == spellInfo->Id)              // skip main spell, already handled above
                {
                    continue;
                }

                AddSpellCooldown(*i_scset, itemId, catrecTime);
            }
        }
    }
}

void SpellCooldownMgr::AddSpellCooldown(uint32 spellid, uint32 itemid, time_t end_time)
{
    SpellCooldown sc;
    sc.end = end_time;
    sc.itemid = itemid;
    m_cooldowns[spellid] = sc;
}

void SpellCooldownMgr::SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId, Spell* spell)
{
    // start cooldowns at server side, if any
    AddSpellAndCategoryCooldowns(spellInfo, itemId, spell);

    // Send activate cooldown timer (possible 0) at client side
    WorldPacket data(SMSG_COOLDOWN_EVENT, (4 + 8));
    data << uint32(spellInfo->Id);
    data << m_owner->GetObjectGuid();
    m_owner->SendDirectMessage(&data);
}

void SpellCooldownMgr::RemoveSpellCooldown(uint32 spell_id, bool update /* = false */)
{
    m_cooldowns.erase(spell_id);

    if (update)
    {
        m_owner->SendClearCooldown(spell_id, m_owner);
    }
}

void SpellCooldownMgr::RemoveSpellCategoryCooldown(uint32 cat, bool update /* = false */)
{
    SpellCategoryStore::const_iterator ct = sSpellCategoryStore.find(cat);
    if (ct == sSpellCategoryStore.end())
    {
        return;
    }

    const SpellCategorySet& ct_set = ct->second;
    for (SpellCooldowns::const_iterator i = m_cooldowns.begin(); i != m_cooldowns.end();)
    {
        if (ct_set.find(i->first) != ct_set.end())
        {
            RemoveSpellCooldown((i++)->first, update);
        }
        else
        {
            ++i;
        }
    }
}

void SpellCooldownMgr::RemoveAllSpellCooldown()
{
    if (!m_cooldowns.empty())
    {
        for (SpellCooldowns::const_iterator itr = m_cooldowns.begin(); itr != m_cooldowns.end(); ++itr)
        {
            m_owner->SendClearCooldown(itr->first, m_owner);
        }

        m_cooldowns.clear();
    }
}

void SpellCooldownMgr::LoadFromDB(QueryResult* result)
{
    // some cooldowns can be already set at aura loading...

    // QueryResult *result = CharacterDatabase.PQuery("SELECT `spell`,`item`,`time` FROM `character_spell_cooldown` WHERE `guid` = '%u'",GetGUIDLow());

    if (result)
    {
        time_t curTime = time(NULL);

        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();
            uint32 item_id  = fields[1].GetUInt32();
            time_t db_time  = (time_t)fields[2].GetUInt64();

            if (!sSpellStore.LookupEntry(spell_id))
            {
                sLog.outError("Player %u has unknown spell %u in `character_spell_cooldown`, skipping.", m_owner->GetGUIDLow(), spell_id);
                continue;
            }

            // skip outdated cooldown
            if (db_time <= curTime)
            {
                continue;
            }

            AddSpellCooldown(spell_id, item_id, db_time);

            DEBUG_LOG("Player (GUID: %u) spell %u, item %u cooldown loaded (%u secs).", m_owner->GetGUIDLow(), spell_id, item_id, uint32(db_time - curTime));
        }
        while (result->NextRow());

        delete result;
    }
}

void SpellCooldownMgr::SaveToDB()
{
    static SqlStatementID deleteSpellCooldown ;
    static SqlStatementID insertSpellCooldown ;

    SqlStatement stmt = CharacterDatabase.CreateStatement(deleteSpellCooldown, "DELETE FROM `character_spell_cooldown` WHERE `guid` = ?");
    stmt.PExecute(m_owner->GetGUIDLow());

    time_t curTime = time(NULL);
    time_t infTime = curTime + Player::infinityCooldownDelayCheck;

    // remove outdated and save active
    for (SpellCooldowns::iterator itr = m_cooldowns.begin(); itr != m_cooldowns.end();)
    {
        if (itr->second.end <= curTime)
        {
            m_cooldowns.erase(itr++);
        }
        else if (itr->second.end <= infTime)                // not save locked cooldowns, it will be reset or set at reload
        {
            stmt = CharacterDatabase.CreateStatement(insertSpellCooldown, "INSERT INTO `character_spell_cooldown` (`guid`,`spell`,`item`,`time`) VALUES( ?, ?, ?, ?)");
            stmt.PExecute(m_owner->GetGUIDLow(), itr->first, itr->second.itemid, uint64(itr->second.end));
            ++itr;
        }
        else
        {
            ++itr;
        }
    }
}
