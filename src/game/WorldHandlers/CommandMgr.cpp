/*
 * Copyright (C) 2015-2022 MaNGOS <https://getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "Common.h"
#include "SharedDefines.h"
#include "Policies/Singleton.h"
#include "ObjectGuid.h"
#include "Language.h"
#include "CommandMgr.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"

class ChatCommand; // Forward declaration of

INSTANTIATE_SINGLETON_1(CommandMgr);

CommandMgr::CommandMgr() {}
CommandMgr::~CommandMgr() {}

// Perhaps migrate all this in ObjectMgr.cpp ?
void CommandMgr::LoadCommandHelpLocale()
{
    m_CommandHelpLocaleMap.clear();

    uint32 count=0;

    QueryResult* result = WorldDatabase.Query("SELECT "
        "`id`,"
        "`help_text_loc1`,"
        "`help_text_loc2`,"
        "`help_text_loc3`,"
        "`help_text_loc4`,"
        "`help_text_loc5`,"
        "`help_text_loc6`,"
        "`help_text_loc7`,"
        "`help_text_loc8`"
        " FROM `locales_command` ORDER BY `id` ASC "
    );

    if (!result)
    {
        sLog.outString();
        sLog.outString(">> Loaded 0 locales command help definitions. DB table `locales_command` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 commandId = fields[0].GetUInt32(); // to assign with db data

        CommandHelpLocale& data = m_CommandHelpLocaleMap[commandId];
        for (int i = 1; i <= MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (!str.empty())
            {
                int idx = sObjectMgr.GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.HelpText.size() <= idx)
                    {
                        data.HelpText.resize(idx + 1);
                    }
                    data.HelpText[idx] = str;
                }
            }
        }

        ++count;
    } while (result->NextRow());

    sLog.outString();
    sLog.outString(">> Loaded %u locale command help definitions", count);

}

CommandHelpLocale const* CommandMgr::GetCommandLocale(uint32 commandId) const
{
    CommandHelpLocaleMap::const_iterator itr = m_CommandHelpLocaleMap.find(commandId);
    if (itr == m_CommandHelpLocaleMap.end())
    {
        return NULL;
    }
    return &itr->second;
}

void CommandMgr::GetCommandHelpLocaleString(uint32 commandId, int32 loc_idx, std::string* namePtr) const
{
    if (loc_idx >= 0)
    {
        if (CommandHelpLocale const* il = GetCommandLocale(commandId))
        {
            if (namePtr && il->HelpText.size() > size_t(loc_idx) && !il->HelpText[loc_idx].empty())
            {
                *namePtr = il->HelpText[loc_idx];
            }
        }
    }
}
