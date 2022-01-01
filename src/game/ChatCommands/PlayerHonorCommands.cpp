/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"

 /**********************************************************************
     CommandTable : honorCommandTable
 /***********************************************************************/

bool ChatHandler::HandleHonorShow(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        target = m_session->GetPlayer();
    }

    int8 highest_rank = target->GetHonorHighestRankInfo().visualRank;
    uint32 dishonorable_kills = target->GetUInt32Value(PLAYER_FIELD_LIFETIME_DISHONORABLE_KILLS);
    uint32 honorable_kills = target->GetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS);
    uint32 today_honorable_kills = target->GetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 0);
    uint32 today_dishonorable_kills = target->GetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 1);
    uint32 yesterday_kills = target->GetUInt32Value(PLAYER_FIELD_YESTERDAY_KILLS);
    uint32 yesterday_honor = target->GetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION);
    uint32 this_week_kills = target->GetUInt32Value(PLAYER_FIELD_THIS_WEEK_KILLS);
    uint32 this_week_honor = target->GetUInt32Value(PLAYER_FIELD_THIS_WEEK_CONTRIBUTION);
    uint32 last_week_kills = target->GetUInt32Value(PLAYER_FIELD_LAST_WEEK_KILLS);
    uint32 last_week_honor = target->GetUInt32Value(PLAYER_FIELD_LAST_WEEK_CONTRIBUTION);
    uint32 last_week_standing = target->GetUInt32Value(PLAYER_FIELD_LAST_WEEK_RANK);

    static int16 alliance_ranks[HONOR_RANK_COUNT] =
    {
        LANG_NO_RANK,
        LANG_RANK_PARIAH,
        LANG_RANK_OUTLAW,
        LANG_RANK_EXILED,
        LANG_RANK_DISHONORED,
        LANG_ALI_PRIVATE,
        LANG_ALI_CORPORAL,
        LANG_ALI_SERGEANT,
        LANG_ALI_MASTER_SERGEANT,
        LANG_ALI_SERGEANT_MAJOR,
        LANG_ALI_KNIGHT,
        LANG_ALI_KNIGHT_LIEUTENANT,
        LANG_ALI_KNIGHT_CAPTAIN,
        LANG_ALI_KNIGHT_CHAMPION,
        LANG_ALI_LIEUTENANT_COMMANDER,
        LANG_ALI_COMMANDER,
        LANG_ALI_MARSHAL,
        LANG_ALI_FIELD_MARSHAL,
        LANG_ALI_GRAND_MARSHAL,
        // LANG_GAME_MASTER
    };
    static int16 horde_ranks[HONOR_RANK_COUNT] =
    {
        LANG_NO_RANK,
        LANG_RANK_PARIAH,
        LANG_RANK_OUTLAW,
        LANG_RANK_EXILED,
        LANG_RANK_DISHONORED,
        LANG_HRD_SCOUT,
        LANG_HRD_GRUNT,
        LANG_HRD_SERGEANT,
        LANG_HRD_SENIOR_SERGEANT,
        LANG_HRD_FIRST_SERGEANT,
        LANG_HRD_STONE_GUARD,
        LANG_HRD_BLOOD_GUARD,
        LANG_HRD_LEGIONNARE,
        LANG_HRD_CENTURION,
        LANG_HRD_CHAMPION,
        LANG_HRD_LIEUTENANT_GENERAL,
        LANG_HRD_GENERAL,
        LANG_HRD_WARLORD,
        LANG_HRD_HIGH_WARLORD,
        // LANG_GAME_MASTER
    };
    char const* rank_name = NULL;
    char const* hrank_name = NULL;

    uint32 honor_rank = target->GetHonorRankInfo().visualRank;

    if (target->GetTeam() == ALLIANCE)
    {
        rank_name = GetMangosString(alliance_ranks[honor_rank]);
        hrank_name = GetMangosString(alliance_ranks[highest_rank]);
    }
    else if (target->GetTeam() == HORDE)
    {
        rank_name = GetMangosString(horde_ranks[honor_rank]);
        hrank_name = GetMangosString(horde_ranks[highest_rank]);
    }
    else
    {
        rank_name = GetMangosString(LANG_NO_RANK);
        hrank_name = GetMangosString(LANG_NO_RANK);
    }

    PSendSysMessage(LANG_RANK, target->GetName(), rank_name, honor_rank);
    PSendSysMessage(LANG_HONOR_TODAY, today_honorable_kills, today_dishonorable_kills);
    PSendSysMessage(LANG_HONOR_YESTERDAY, yesterday_kills, yesterday_honor);
    PSendSysMessage(LANG_HONOR_THIS_WEEK, this_week_kills, this_week_honor);
    PSendSysMessage(LANG_HONOR_LAST_WEEK, last_week_kills, last_week_honor, last_week_standing);
    PSendSysMessage(LANG_HONOR_LIFE, target->GetRankPoints(), honorable_kills, dishonorable_kills, highest_rank, hrank_name);

    return true;
}

bool ChatHandler::HandleHonorAddCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
    {
        return false;
    }

    float amount = (float)atof(args);
    target->SetStoredHonor(target->GetStoredHonor() + amount);
    target->UpdateHonor();
    return true;
}

bool ChatHandler::HandleHonorAddKillCommand(char* /*args*/)
{
    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // check online security
    if (target == m_session->GetPlayer())
    {
        return false;
    }

    m_session->GetPlayer()->RewardHonor(target, 1);
    return true;
}

bool ChatHandler::HandleHonorUpdateCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    target->UpdateHonor();
    return true;
}

/**********************************************************************
    CommandTable : modifyCommandTable
/***********************************************************************/

bool ChatHandler::HandleModifyHonorCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    char* field = ExtractLiteralArg(&args);
    if (!field)
    {
        return false;
    }

    int32 amount;
    if (!ExtractInt32(&args, amount))
    {
        return false;
    }

    // hack code
    if (hasStringAbbr(field, "points"))
    {
        if (amount < 0 || amount > 255)
        {
            return false;
        }
        // rank points is sent to client with same size of uint8(255) for each rank
        target->SetByteValue(PLAYER_FIELD_BYTES2, 0, amount);
    }
    else if (hasStringAbbr(field, "rank"))
    {
        if (amount < 0 || amount >= HONOR_RANK_COUNT)
        {
            return false;
        }
        target->SetByteValue(PLAYER_BYTES_3, 3, amount);
    }
    else if (hasStringAbbr(field, "todaykills"))
    {
        target->SetUInt16Value(PLAYER_FIELD_SESSION_KILLS, 0, (uint32)amount);
    }
    else if (hasStringAbbr(field, "yesterdaykills"))
    {
        target->SetUInt32Value(PLAYER_FIELD_YESTERDAY_KILLS, (uint32)amount);
    }
    else if (hasStringAbbr(field, "yesterdayhonor"))
    {
        target->SetUInt32Value(PLAYER_FIELD_YESTERDAY_CONTRIBUTION, (uint32)amount);
    }
    else if (hasStringAbbr(field, "thisweekkills"))
    {
        target->SetUInt32Value(PLAYER_FIELD_THIS_WEEK_KILLS, (uint32)amount);
    }
    else if (hasStringAbbr(field, "thisweekhonor"))
    {
        target->SetUInt32Value(PLAYER_FIELD_THIS_WEEK_CONTRIBUTION, (uint32)amount);
    }
    else if (hasStringAbbr(field, "lastweekkills"))
    {
        target->SetUInt32Value(PLAYER_FIELD_LAST_WEEK_KILLS, (uint32)amount);
    }
    else if (hasStringAbbr(field, "lastweekhonor"))
    {
        target->SetUInt32Value(PLAYER_FIELD_LAST_WEEK_CONTRIBUTION, (uint32)amount);
    }
    else if (hasStringAbbr(field, "lastweekstanding"))
    {
        target->SetUInt32Value(PLAYER_FIELD_LAST_WEEK_RANK, (uint32)amount);
    }
    else if (hasStringAbbr(field, "lifetimedishonorablekills"))
    {
        target->SetUInt32Value(PLAYER_FIELD_LIFETIME_DISHONORABLE_KILLS, (uint32)amount);
    }
    else if (hasStringAbbr(field, "lifetimehonorablekills"))
    {
        target->SetUInt32Value(PLAYER_FIELD_LIFETIME_HONORABLE_KILLS, (uint32)amount);
    }

    PSendSysMessage(LANG_COMMAND_MODIFY_HONOR, field, target->GetName(), hasStringAbbr(field, "rank") ? amount : (uint32)amount);

    return true;
}

/**********************************************************************
    CommandTable : resetCommandTable
/***********************************************************************/

bool ChatHandler::HandleResetHonorCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    target->ResetHonor();
    return true;
}

