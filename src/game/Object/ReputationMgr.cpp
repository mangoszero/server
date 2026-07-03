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

#include "ReputationMgr.h"
#include "DBCStores.h"
#include "Player.h"
#include "WorldPacket.h"
#include "ObjectMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

const int32 ReputationMgr::PointsInRank[MAX_REPUTATION_RANK] = {36000, 3000, 3000, 3000, 6000, 12000, 21000, 1000};

/**
 * @brief Converts a raw reputation value into a reputation rank.
 *
 * @param standing The raw reputation standing.
 * @return The matching reputation rank.
 */
ReputationRank ReputationMgr::ReputationToRank(int32 standing)
{
    int32 limit = Reputation_Cap + 1;
    for (int i = MAX_REPUTATION_RANK - 1; i >= MIN_REPUTATION_RANK; --i)
    {
        limit -= PointsInRank[i];
        if (standing >= limit)
        {
            return ReputationRank(i);
        }
    }
    return MIN_REPUTATION_RANK;
}

/**
 * @brief Gets a player's reputation for a faction by faction ID.
 *
 * @param faction_id The faction identifier.
 * @return The effective reputation value.
 */
int32 ReputationMgr::GetReputation(uint32 faction_id) const
{
    FactionEntry const* factionEntry = sFactionStore.LookupEntry(faction_id);

    if (!factionEntry)
    {
        sLog.outError("ReputationMgr::GetReputation: Can't get reputation of %s for unknown faction (faction id) #%u.", m_player->GetName(), faction_id);
        return 0;
    }

    return GetReputation(factionEntry);
}

/**
 * @brief Gets the base reputation for a faction based on race and class.
 *
 * @param factionEntry The faction entry to inspect.
 * @return The base reputation value.
 */
int32 ReputationMgr::GetBaseReputation(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
    {
        return 0;
    }

    uint32 raceMask = m_player->getRaceMask();
    uint32 classMask = m_player->getClassMask();

    int idx = factionEntry->GetIndexFitTo(raceMask, classMask);

    return idx >= 0 ? factionEntry->ReputationBase[idx] : 0;
}

/**
 * @brief Gets the effective reputation for a faction entry.
 *
 * @param factionEntry The faction entry to inspect.
 * @return The effective reputation value.
 */
int32 ReputationMgr::GetReputation(FactionEntry const* factionEntry) const
{
    // Faction without recorded reputation. Just ignore.
    if (!factionEntry)
    {
        return 0;
    }

    if (FactionState const* state = GetState(factionEntry))
    {
        return GetBaseReputation(factionEntry) + state->Standing;
    }

    return 0;
}

/**
 * @brief Gets the current reputation rank for a faction.
 *
 * @param factionEntry The faction entry to inspect.
 * @return The current reputation rank.
 */
ReputationRank ReputationMgr::GetRank(FactionEntry const* factionEntry) const
{
    int32 reputation = GetReputation(factionEntry);
    return ReputationToRank(reputation);
}

/**
 * @brief Gets the base reputation rank for a faction.
 *
 * @param factionEntry The faction entry to inspect.
 * @return The base reputation rank.
 */
ReputationRank ReputationMgr::GetBaseRank(FactionEntry const* factionEntry) const
{
    int32 reputation = GetBaseReputation(factionEntry);
    return ReputationToRank(reputation);
}

/**
 * @brief Applies or removes a forced reaction rank for a faction.
 *
 * @param faction_id The faction identifier.
 * @param rank The forced rank to apply.
 * @param apply true to apply the forced reaction; false to remove it.
 */
void ReputationMgr::ApplyForceReaction(uint32 faction_id, ReputationRank rank, bool apply)
{
    if (apply)
    {
        m_forcedReactions[faction_id] = rank;
    }
    else
    {
        m_forcedReactions.erase(faction_id);
    }
}

/**
 * @brief Gets the default reputation state flags for a faction.
 *
 * @param factionEntry The faction entry to inspect.
 * @return The default reputation flags.
 */
uint32 ReputationMgr::GetDefaultStateFlags(FactionEntry const* factionEntry) const
{
    if (!factionEntry)
    {
        return 0;
    }

    uint32 raceMask = m_player->getRaceMask();
    uint32 classMask = m_player->getClassMask();

    int idx = factionEntry->GetIndexFitTo(raceMask, classMask);

    return idx >= 0 ? factionEntry->ReputationFlags[idx] : 0;
}

/**
 * @brief Sends forced reaction states to the client.
 */
void ReputationMgr::SendForceReactions()
{
    WorldPacket data;
    data.Initialize(SMSG_SET_FORCED_REACTIONS, 4 + m_forcedReactions.size() * (4 + 4));
    data << uint32(m_forcedReactions.size());
    for (ForcedReactions::const_iterator itr = m_forcedReactions.begin(); itr != m_forcedReactions.end(); ++itr)
    {
        data << uint32(itr->first);                         // faction_id (Faction.dbc)
        data << uint32(itr->second);                        // reputation rank
    }
    m_player->SendDirectMessage(&data);
}

/**
 * @brief Sends updated faction standing data to the client.
 *
 * @param faction The primary faction state being reported.
 */
void ReputationMgr::SendState(FactionState const* faction)
{
    uint32 count = 1;

    WorldPacket data(SMSG_SET_FACTION_STANDING, (16));      // last check 2.4.0
    size_t p_count = data.wpos();
    data << (uint32) count;                                 // placeholder

    data << (uint32) faction->ReputationListID;
    data << (uint32) faction->Standing;

    for (FactionStateList::iterator itr = m_factions.begin(); itr != m_factions.end(); ++itr)
    {
        FactionState &subFaction = itr->second;
        if (subFaction.needSend)
        {
            subFaction.needSend = false;
            if (subFaction.ReputationListID != faction->ReputationListID)
            {
                data << uint32(subFaction.ReputationListID);
                data << uint32(subFaction.Standing);

                ++count;
            }
        }
    }

    data.put<uint32>(p_count, count);
    m_player->SendDirectMessage(&data);
}

struct rep
{
    uint8 flags;
    uint32 standing;
};

/* Called from Player::SendInitialPacketsBeforeAddToMap */

/**
 * @brief Sends the initial reputation table to the client.
 */
void ReputationMgr::SendInitialReputations()
{
    WorldPacket data(SMSG_INITIALIZE_FACTIONS, (4 + 64 * 5));
    data << uint32(0x00000040);

    RepListID a = 0;

    for (FactionStateList::iterator itr = m_factions.begin(); itr != m_factions.end(); ++itr)
    {
        // fill in absent fields
        for (; a != itr->first; ++a)
        {
            data << uint8(0x00);
            data << uint32(0x00000000);
        }

        // fill in encountered data
        data << uint8(itr->second.Flags);
        data << uint32(itr->second.Standing);

        itr->second.needSend = false;

        ++a;
    }

    // fill in absent fields
    for (; a != 64; ++a)
    {
        data << uint8(0x00);
        data << uint32(0x00000000);
    }

    m_player->SendDirectMessage(&data);
}

/**
 * @brief Marks a faction as visible in the client reputation list.
 *
 * @param faction The faction state to reveal.
 */
void ReputationMgr::SendVisible(FactionState const* faction) const
{
    if (m_player->GetSession()->PlayerLoading())
    {
        return;
    }

    // make faction visible in reputation list at client
    WorldPacket data(SMSG_SET_FACTION_VISIBLE, 4);
    data << faction->ReputationListID;
    m_player->SendDirectMessage(&data);
}

/**
 * @brief Initializes all tracked faction states for the player.
 */
void ReputationMgr::Initialize()
{
    m_factions.clear();

    for (unsigned int i = 1; i < sFactionStore.GetNumRows(); ++i)
    {
        FactionEntry const* factionEntry = sFactionStore.LookupEntry(i);

        if (factionEntry && (factionEntry->ReputationIndex >= 0))
        {
            FactionState newFaction;
            newFaction.ID = factionEntry->ID;
            newFaction.ReputationListID = factionEntry->ReputationIndex;
            newFaction.Standing = 0;
            newFaction.Flags = GetDefaultStateFlags(factionEntry);
            newFaction.needSend = true;
            newFaction.needSave = true;

            m_factions[newFaction.ReputationListID] = newFaction;
        }
    }
}

/**
 * @brief Sets reputation for a faction and applies spillover if configured.
 *
 * @param factionEntry The faction entry to modify.
 * @param standing The new reputation value or delta.
 * @param incremental true if the standing value is a delta; otherwise, false.
 * @return true if the faction state was updated; otherwise, false.
 */
bool ReputationMgr::SetReputation(FactionEntry const* factionEntry, int32 standing, bool incremental)
{
    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = m_player->GetEluna())
    {
        e->OnReputationChange(m_player, factionEntry->ID, standing, incremental);
    }
#endif /* ENABLE_ELUNA */

    bool res = false;
    // if spillover definition exists in DB, override DBC
    if (const RepSpilloverTemplate* repTemplate = sObjectMgr.GetRepSpilloverTemplate(factionEntry->ID))
    {
        for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
        {
            if (repTemplate->faction[i])
            {
                if (m_player->GetReputationRank(repTemplate->faction[i]) <= ReputationRank(repTemplate->faction_rank[i]))
                {
                    // bonuses are already given, so just modify standing by rate
                    int32 spilloverRep = standing * repTemplate->faction_rate[i];
                    SetOneFactionReputation(sFactionStore.LookupEntry(repTemplate->faction[i]), spilloverRep, incremental);
                }
            }
        }
    }
    // spillover done, update faction itself
    FactionStateList::iterator faction = m_factions.find(factionEntry->ReputationIndex);
    if (faction != m_factions.end())
    {
        res = SetOneFactionReputation(factionEntry, standing, incremental);
        // only this faction gets reported to client, even if it has no own visible standing
        SendState(&faction->second);
    }
    return res;
}

/**
 * @brief Sets reputation for a single faction without spillover handling.
 *
 * @param factionEntry The faction entry to modify.
 * @param standing The new reputation value or delta.
 * @param incremental true if the standing value is a delta; otherwise, false.
 * @return true if the faction state was updated; otherwise, false.
 */
bool ReputationMgr::SetOneFactionReputation(FactionEntry const* factionEntry, int32 standing, bool incremental)
{
    FactionStateList::iterator itr = m_factions.find(factionEntry->ReputationIndex);
    if (itr != m_factions.end())
    {
        FactionState &faction = itr->second;
        int32 BaseRep = GetBaseReputation(factionEntry);

        if (incremental)
        {
            standing += faction.Standing + BaseRep;
        }

        if (standing > Reputation_Cap)
        {
            standing = Reputation_Cap;
        }
        else if (standing < Reputation_Bottom)
        {
            standing = Reputation_Bottom;
        }

        faction.Standing = standing - BaseRep;
        faction.needSend = true;
        faction.needSave = true;

        SetVisible(&faction);

        // check and, if needed, modify AtWar flag every rep rank crossing
        if (ReputationToRank(standing) != ReputationToRank(BaseRep))
        {
            SetAtWar(&itr->second, ReputationToRank(standing) <= REP_HOSTILE);
        }

        m_player->ReputationChanged(factionEntry);

        return true;
    }
    return false;
}

/**
 * @brief Makes the faction referenced by a faction template visible.
 *
 * @param factionTemplateEntry The faction template entry to inspect.
 */
void ReputationMgr::SetVisible(FactionTemplateEntry const* factionTemplateEntry)
{
    if (!factionTemplateEntry->Faction)
    {
        return;
    }

    if (FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionTemplateEntry->Faction))
    {
        SetVisible(factionEntry);
    }
}

/**
 * @brief Marks a faction visible using its faction entry.
 *
 * @param factionEntry The faction entry to reveal.
 */
void ReputationMgr::SetVisible(FactionEntry const* factionEntry)
{
    if (factionEntry->ReputationIndex < 0)
    {
        return;
    }

    FactionStateList::iterator itr = m_factions.find(factionEntry->ReputationIndex);
    if (itr == m_factions.end())
    {
        return;
    }

    SetVisible(&itr->second);
}

/**
 * @brief Marks a faction state as visible and schedules updates.
 *
 * @param faction The faction state to reveal.
 */
void ReputationMgr::SetVisible(FactionState* faction)
{
    // always invisible or hidden faction can't be make visible
    if (faction->Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN))
    {
        return;
    }

    // already set
    if (faction->Flags & FACTION_FLAG_VISIBLE)
    {
        return;
    }

    faction->Flags |= FACTION_FLAG_VISIBLE;
    faction->needSend = true;
    faction->needSave = true;

    SendVisible(faction);
}

/**
 * @brief Sets or clears the at-war flag for a faction list id.
 *
 * @param repListID The reputation list identifier.
 * @param on true to declare war; false to clear it.
 */
void ReputationMgr::SetAtWar(RepListID repListID, bool on)
{
    FactionStateList::iterator itr = m_factions.find(repListID);
    if (itr == m_factions.end())
    {
        return;
    }

    // always invisible or hidden faction can't change war state
    if (itr->second.Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN))
    {
        return;
    }

    SetAtWar(&itr->second, on);
}

/**
 * @brief Sets or clears the at-war flag for a faction state.
 *
 * @param faction The faction state to update.
 * @param atWar true to declare war; false to clear it.
 */
void ReputationMgr::SetAtWar(FactionState* faction, bool atWar)
{
    // not allow declare war to faction unless already hated or less
    if (atWar && (faction->Flags & FACTION_FLAG_PEACE_FORCED) && ReputationToRank(faction->Standing) > REP_HATED)
    {
        return;
    }

    // already set
    if (((faction->Flags & FACTION_FLAG_AT_WAR) != 0) == atWar)
    {
        return;
    }

    if (atWar)
    {
        faction->Flags |= FACTION_FLAG_AT_WAR;
    }
    else
    {
        faction->Flags &= ~FACTION_FLAG_AT_WAR;
    }

    faction->needSend = true;
    faction->needSave = true;
}

/**
 * @brief Sets or clears inactive status for a faction list id.
 *
 * @param repListID The reputation list identifier.
 * @param on true to mark inactive; false to activate.
 */
void ReputationMgr::SetInactive(RepListID repListID, bool on)
{
    FactionStateList::iterator itr = m_factions.find(repListID);
    if (itr == m_factions.end())
    {
        return;
    }

    SetInactive(&itr->second, on);
}

/**
 * @brief Sets or clears inactive status for a faction state.
 *
 * @param faction The faction state to update.
 * @param inactive true to mark inactive; false to activate it.
 */
void ReputationMgr::SetInactive(FactionState* faction, bool inactive)
{
    // always invisible or hidden faction can't be inactive
    if (inactive && ((faction->Flags & (FACTION_FLAG_INVISIBLE_FORCED | FACTION_FLAG_HIDDEN)) || !(faction->Flags & FACTION_FLAG_VISIBLE)))
    {
        return;
    }

    // already set
    if (((faction->Flags & FACTION_FLAG_INACTIVE) != 0) == inactive)
    {
        return;
    }

    if (inactive)
    {
        faction->Flags |= FACTION_FLAG_INACTIVE;
    }
    else
    {
        faction->Flags &= ~FACTION_FLAG_INACTIVE;
    }

    faction->needSend = true;
    faction->needSave = true;
}

/**
 * @brief Loads saved reputation standings and flags from the database.
 *
 * @param result The query result containing saved faction rows.
 */
void ReputationMgr::LoadFromDB(QueryResult* result)
{
    // Set initial reputations (so everything is nifty before DB data load)
    Initialize();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT `faction`,`standing`,`flags` FROM character_reputation WHERE guid = '%u'",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            FactionEntry const* factionEntry = sFactionStore.LookupEntry(fields[0].GetUInt32());
            if (factionEntry && (factionEntry->ReputationIndex >= 0))
            {
                FactionState* faction = &m_factions[factionEntry->ReputationIndex];

                // update standing to current
                faction->Standing = int32(fields[1].GetUInt32());

                uint32 dbFactionFlags = fields[2].GetUInt32();

                if (dbFactionFlags & FACTION_FLAG_VISIBLE)
                {
                    SetVisible(faction); // have internal checks for forced invisibility
                }

                if (dbFactionFlags & FACTION_FLAG_INACTIVE)
                {
                    SetInactive(faction, true); // have internal checks for visibility requirement
                }

                if (dbFactionFlags & FACTION_FLAG_AT_WAR)   // DB at war
                {
                    SetAtWar(faction, true); // have internal checks for FACTION_FLAG_PEACE_FORCED
                }
                else                                        // DB not at war
                {
                    // allow remove if visible (and then not FACTION_FLAG_INVISIBLE_FORCED or FACTION_FLAG_HIDDEN)
                    if (faction->Flags & FACTION_FLAG_VISIBLE)
                    {
                        SetAtWar(faction, false); // have internal checks for FACTION_FLAG_PEACE_FORCED
                    }
                }

                // set atWar for hostile
                ForcedReactions::const_iterator forceItr = m_forcedReactions.find(factionEntry->ID);
                if (forceItr != m_forcedReactions.end())
                {
                    if (forceItr->second <= REP_HOSTILE)
                    {
                        SetAtWar(faction, true);
                    }
                }
                else if (GetRank(factionEntry) <= REP_HOSTILE)
                {
                    SetAtWar(faction, true);
                }

                // reset changed flag if values similar to saved in DB
                if (faction->Flags == dbFactionFlags)
                {
                    faction->needSend = false;
                    faction->needSave = false;
                }
            }
        }
        while (result->NextRow());

        delete result;
    }
}

/**
 * @brief Saves changed reputation standings and flags to the database.
 */
void ReputationMgr::SaveToDB()
{
    static SqlStatementID delRep ;
    static SqlStatementID insRep ;

    SqlStatement stmtDel = CharacterDatabase.CreateStatement(delRep, "DELETE FROM `character_reputation` WHERE `guid` = ? AND `faction`=?");
    SqlStatement stmtIns = CharacterDatabase.CreateStatement(insRep, "INSERT INTO `character_reputation` (`guid`,`faction`,`standing`,`flags`) VALUES (?, ?, ?, ?)");

    for (FactionStateList::iterator itr = m_factions.begin(); itr != m_factions.end(); ++itr)
    {
        FactionState &faction = itr->second;
        if (faction.needSave)
        {
            stmtDel.PExecute(m_player->GetGUIDLow(), faction.ID);
            stmtIns.PExecute(m_player->GetGUIDLow(), faction.ID, faction.Standing, faction.Flags);
            faction.needSave = false;
        }
    }
}
