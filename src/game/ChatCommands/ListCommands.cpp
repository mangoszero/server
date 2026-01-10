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

#include "Chat.h"
#include "ObjectMgr.h"
#include "SpellAuras.h"

 /**********************************************************************
      CommandTable : listCommandTable
 /***********************************************************************/

bool ChatHandler::HandleListAurasCommand(char* /*args*/)
{
    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    char const* talentStr = GetMangosString(LANG_TALENT);
    char const* passiveStr = GetMangosString(LANG_PASSIVE);

    Unit::SpellAuraHolderMap const& uAuras = unit->GetSpellAuraHolderMap();
    PSendSysMessage(LANG_COMMAND_TARGET_LISTAURAS, uAuras.size());
    for (Unit::SpellAuraHolderMap::const_iterator itr = uAuras.begin(); itr != uAuras.end(); ++itr)
    {
        bool talent = GetTalentSpellCost(itr->second->GetId()) > 0;

        SpellAuraHolder* holder = itr->second;
        char const* name = holder->GetSpellProto()->SpellName[GetSessionDbcLocale()];

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i));
            if (!aur)
            {
                continue;
            }

            if (m_session)
            {
                std::ostringstream ss_name;
                ss_name << "|cffffffff|Hspell:" << itr->second->GetId() << "|h[" << name << "]|h|r";

                PSendSysMessage(LANG_COMMAND_TARGET_AURADETAIL, holder->GetId(), aur->GetEffIndex(),
                                aur->GetModifier()->m_auraname, aur->GetAuraDuration(), aur->GetAuraMaxDuration(),
                                ss_name.str().c_str(),
                                (holder->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                holder->GetCasterGuid().GetString().c_str(), aur->GetStackAmount());
            }
            else
            {
                PSendSysMessage(LANG_COMMAND_TARGET_AURADETAIL, holder->GetId(), aur->GetEffIndex(),
                                aur->GetModifier()->m_auraname, aur->GetAuraDuration(), aur->GetAuraMaxDuration(),
                                name,
                                (holder->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                holder->GetCasterGuid().GetString().c_str(), aur->GetStackAmount());
            }
        }
    }
    for (int i = 0; i < TOTAL_AURAS; ++i)
    {
        Unit::AuraList const& uAuraList = unit->GetAurasByType(AuraType(i));
        if (uAuraList.empty())
        {
            continue;
        }
        PSendSysMessage(LANG_COMMAND_TARGET_LISTAURATYPE, uAuraList.size(), i);
        for (Unit::AuraList::const_iterator itr = uAuraList.begin(); itr != uAuraList.end(); ++itr)
        {
            bool talent = GetTalentSpellCost((*itr)->GetId()) > 0;

            char const* name = (*itr)->GetSpellProto()->SpellName[GetSessionDbcLocale()];

            if (m_session)
            {
                std::ostringstream ss_name;
                ss_name << "|cffffffff|Hspell:" << (*itr)->GetId() << "|h[" << name << "]|h|r";

                PSendSysMessage(LANG_COMMAND_TARGET_AURASIMPLE, (*itr)->GetId(), (*itr)->GetEffIndex(),
                                ss_name.str().c_str(), ((*itr)->GetHolder()->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                (*itr)->GetCasterGuid().GetString().c_str());
            }
            else
            {
                PSendSysMessage(LANG_COMMAND_TARGET_AURASIMPLE, (*itr)->GetId(), (*itr)->GetEffIndex(),
                                name, ((*itr)->GetHolder()->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                (*itr)->GetCasterGuid().GetString().c_str());
            }
        }
    }
    return true;
}

bool ChatHandler::HandleListTalentsCommand(char* /*args*/)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    SendSysMessage(LANG_LIST_TALENTS_TITLE);
    uint32 count = 0;
    uint32 cost = 0;
    PlayerSpellMap const& uSpells = player->GetSpellMap();
    for (PlayerSpellMap::const_iterator itr = uSpells.begin(); itr != uSpells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled)
        {
            continue;
        }

        uint32 cost_itr = GetTalentSpellCost(itr->first);

        if (cost_itr == 0)
        {
            continue;
        }

        SpellEntry const* spellEntry = sSpellStore.LookupEntry(itr->first);
        if (!spellEntry)
        {
            continue;
        }

        ShowSpellListHelper(player, spellEntry, GetSessionDbcLocale());
        ++count;
        cost += cost_itr;
    }
    PSendSysMessage(LANG_LIST_TALENTS_COUNT, count, cost);

    return true;
}

bool ChatHandler::HandleListItemCommand(char* args)
{
    uint32 item_id;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", item_id))
    {
        return false;
    }

    if (!item_id)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(item_id);
    if (!itemProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
    {
        return false;
    }

    QueryResult* result;

    // inventory case
    uint32 inv_count = 0;
    result = CharacterDatabase.PQuery("SELECT COUNT(`item_template`) FROM `character_inventory` WHERE `item_template`='%u'", item_id);
    if (result)
    {
        inv_count = (*result)[0].GetUInt32();
        delete result;
    }

    result = CharacterDatabase.PQuery(
                 //          0        1             2             3        4                  5
                 "SELECT `ci`.`item`, `cibag`.`slot` AS bag, `ci`.`slot`, `ci`.`guid`, `characters`.`account`,`characters`.`name` "
                 "FROM `character_inventory` AS `ci` LEFT JOIN `character_inventory` AS cibag ON (`cibag`.`item`=`ci`.`bag`),`characters` "
                 "WHERE `ci`.`item_template`='%u' AND `ci`.`guid` = `characters`.`guid` LIMIT %u ",
                 item_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 item_bag = fields[1].GetUInt32();
            uint32 item_slot = fields[2].GetUInt32();
            uint32 owner_guid = fields[3].GetUInt32();
            uint32 owner_acc = fields[4].GetUInt32();
            std::string owner_name = fields[5].GetCppString();

            char const* item_pos = 0;
            if (Player::IsEquipmentPos(item_bag, item_slot))
            {
                item_pos = "[equipped]";
            }
            else if (Player::IsInventoryPos(item_bag, item_slot))
            {
                item_pos = "[in inventory]";
            }
            else if (Player::IsBankPos(item_bag, item_slot))
            {
                item_pos = "[in bank]";
            }
            else
            {
                item_pos = "";
            }

            PSendSysMessage(LANG_ITEMLIST_SLOT,
                            item_guid, owner_name.c_str(), owner_guid, owner_acc, item_pos);
        }
        while (result->NextRow());

        uint32 res_count = uint32(result->GetRowCount());

        delete result;

        if (count > res_count)
        {
            count -= res_count;
        }
        else if (count)
        {
            count = 0;
        }
    }

    // mail case
    uint32 mail_count = 0;
    result = CharacterDatabase.PQuery("SELECT COUNT(`item_template`) FROM `mail_items` WHERE `item_template`='%u'", item_id);
    if (result)
    {
        mail_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (count > 0)
    {
        result = CharacterDatabase.PQuery(
                     //          0                     1            2              3               4            5               6
                     "SELECT `mail_items`.`item_guid`, `mail`.`sender`, `mail`.`receiver`, `char_s`.`account`, `char_s`.`name`, `char_r`.`account`, `char_r`.`name` "
                     "FROM `mail`,`mail_items`,`characters` as char_s,`characters` as char_r "
                     "WHERE `mail_items`.`item_template`='%u' AND `char_s`.`guid` = `mail`.`sender` AND `char_r`.`guid` = `mail`.`receiver` AND `mail`.`id`=`mail_items`.`mail_id` LIMIT %u",
                     item_id, uint32(count));
    }
    else
    {
        result = NULL;
    }

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 item_s = fields[1].GetUInt32();
            uint32 item_r = fields[2].GetUInt32();
            uint32 item_s_acc = fields[3].GetUInt32();
            std::string item_s_name = fields[4].GetCppString();
            uint32 item_r_acc = fields[5].GetUInt32();
            std::string item_r_name = fields[6].GetCppString();

            char const* item_pos = "[in mail]";

            PSendSysMessage(LANG_ITEMLIST_MAIL,
                            item_guid, item_s_name.c_str(), item_s, item_s_acc, item_r_name.c_str(), item_r, item_r_acc, item_pos);
        }
        while (result->NextRow());

        uint32 res_count = uint32(result->GetRowCount());

        delete result;

        if (count > res_count)
        {
            count -= res_count;
        }
        else if (count)
        {
            count = 0;
        }
    }

    // auction case
    uint32 auc_count = 0;
    result = CharacterDatabase.PQuery("SELECT COUNT(`item_template`) FROM `auction` WHERE `item_template`='%u'", item_id);
    if (result)
    {
        auc_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (count > 0)
    {
        result = CharacterDatabase.PQuery(
                     //           0                      1                       2                   3
                     "SELECT  `auction`.`itemguid`, `auction`.`itemowner`, `characters`.`account`, `characters`.`name` "
                     "FROM `auction`,`characters` WHERE `auction`.`item_template`='%u' AND `characters`.`guid` = `auction`.`itemowner` LIMIT %u",
                     item_id, uint32(count));
    }
    else
    {
        result = NULL;
    }

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid = fields[0].GetUInt32();
            uint32 owner = fields[1].GetUInt32();
            uint32 owner_acc = fields[2].GetUInt32();
            std::string owner_name = fields[3].GetCppString();

            char const* item_pos = "[in auction]";

            PSendSysMessage(LANG_ITEMLIST_AUCTION, item_guid, owner_name.c_str(), owner, owner_acc, item_pos);
        }
        while (result->NextRow());

        delete result;
    }

    if (inv_count + mail_count + auc_count == 0)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_COMMAND_LISTITEMMESSAGE, item_id, inv_count + mail_count + auc_count, inv_count, mail_count, auc_count);

    return true;
}

bool ChatHandler::HandleListPlayersCommand(char* args)
{
    uint32 limit;
    if (!ExtractOptUInt32(&args, limit, 100))
    {
        return false;
    }

    uint32 count = 0;

    PSendSysMessage("Online Players (Limit %u):", limit);
    PSendSysMessage("===========================================");

    sObjectAccessor.DoForAllPlayers([&](Player* player)
    {
        if (count >= limit)
        {
            return;
        }

        uint32 mapId = player->GetMapId();
        uint32 zoneId = player->GetZoneId();

        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zoneId);

        PSendSysMessage("%-20s | Lvl %-2u | Map %u Zone %u (%s)",
                       player->GetName(),
                       player->getLevel(),
                       mapId,
                       zoneId,
                       (zoneEntry ? zoneEntry->area_name[GetSessionDbcLocale()] : "Unknown"));

        count++;
    });

    return true;
}

bool ChatHandler::HandleListObjectCommand(char* args)
{
    // number or [name] Shift-click form |color|Hgameobject_entry:go_id|h[name]|h|r
    uint32 go_id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject_entry", go_id))
    {
        return false;
    }

    if (!go_id)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(go_id);
    if (!gInfo)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
    {
        return false;
    }

    QueryResult* result;

    uint32 obj_count = 0;
    result = WorldDatabase.PQuery("SELECT COUNT(`guid`) FROM `gameobject` WHERE `id`='%u'", go_id);
    if (result)
    {
        obj_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (m_session)
    {
        Player* pl = m_session->GetPlayer();
        result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `gameobject` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u",
                                      pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(), go_id, uint32(count));
    }
    else
        result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `gameobject` WHERE `id` = '%u' LIMIT %u",
                                      go_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            int mapid = fields[4].GetUInt16();

            if (m_session)
            {
                PSendSysMessage(LANG_GO_LIST_CHAT, guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(guid).c_str(), guid, gInfo->name, x, y, z, mapid);
            }
            else
            {
                PSendSysMessage(LANG_GO_LIST_CONSOLE, guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(guid).c_str(), gInfo->name, x, y, z, mapid);
            }
        }
        while (result->NextRow());

        delete result;
    }

    PSendSysMessage(LANG_COMMAND_LISTOBJMESSAGE, go_id, obj_count);
    return true;
}

bool ChatHandler::HandleListCreatureCommand(char* args)
{
    // number or [name] Shift-click form |color|Hcreature_entry:creature_id|h[name]|h|r
    uint32 cr_id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", cr_id))
    {
        return false;
    }

    if (!cr_id)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(cr_id);
    if (!cInfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
    {
        return false;
    }

    QueryResult* result;

    uint32 cr_count = 0;
    result = WorldDatabase.PQuery("SELECT COUNT(`guid`) FROM `creature` WHERE `id`='%u'", cr_id);
    if (result)
    {
        cr_count = (*result)[0].GetUInt32();
        delete result;
    }

    if (m_session)
    {
        Player* pl = m_session->GetPlayer();
        result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map`, (POW(`position_x` - '%f', 2) + POW(`position_y` - '%f', 2) + POW(`position_z` - '%f', 2)) AS order_ FROM `creature` WHERE `id` = '%u' ORDER BY `order_` ASC LIMIT %u",
                                      pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(), cr_id, uint32(count));
    }
    else
        result = WorldDatabase.PQuery("SELECT `guid`, `position_x`, `position_y`, `position_z`, `map` FROM `creature` WHERE `id` = '%u' LIMIT %u",
                                      cr_id, uint32(count));

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            float x = fields[1].GetFloat();
            float y = fields[2].GetFloat();
            float z = fields[3].GetFloat();
            int mapid = fields[4].GetUInt16();

            if (m_session)
            {
                PSendSysMessage(LANG_CREATURE_LIST_CHAT, guid, PrepareStringNpcOrGoSpawnInformation<Creature>(guid).c_str(), guid, cInfo->Name, x, y, z, mapid);
            }
            else
            {
                PSendSysMessage(LANG_CREATURE_LIST_CONSOLE, guid, PrepareStringNpcOrGoSpawnInformation<Creature>(guid).c_str(), cInfo->Name, x, y, z, mapid);
            }
        }
        while (result->NextRow());

        delete result;
    }

    PSendSysMessage(LANG_COMMAND_LISTCREATUREMESSAGE, cr_id, cr_count);
    return true;
}
