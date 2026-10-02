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

#include "AuctionHouseModule.h"
#include "Log.h"

#include <algorithm>

namespace
{
    std::vector<AuctionHouseModules::Factory>& Factories()
    {
        static std::vector<AuctionHouseModules::Factory> factories;
        return factories;
    }

    std::vector<std::unique_ptr<AuctionHouseModule>>& Modules()
    {
        static std::vector<std::unique_ptr<AuctionHouseModule>> modules;
        return modules;
    }

    std::string& ConfigPath()
    {
        static std::string path;
        return path;
    }
}

void AuctionHouseModules::Provide(Factory factory)
{
    Factories().push_back(factory);
}

void AuctionHouseModules::LoadConfig(bool reload)
{
    if (reload)
    {
        for (std::unique_ptr<AuctionHouseModule>& module : Modules())
        {
            module->LoadConfig(true);
        }
        return;
    }

    Modules().clear();
    for (Factory factory : Factories())
    {
        if (AuctionHouseModule* module = factory())
        {
            module->LoadConfig(false);
            sLog.outString("Auction module %s enabled", module->Name());
            Modules().emplace_back(module);
        }
    }
}

void AuctionHouseModules::Start()
{
    for (std::unique_ptr<AuctionHouseModule>& module : Modules())
    {
        module->Start();
    }
}

void AuctionHouseModules::Update(uint32 diff)
{
    for (std::unique_ptr<AuctionHouseModule>& module : Modules())
    {
        module->Update(diff);
    }
}

void AuctionHouseModules::Stop()
{
    for (auto itr = Modules().rbegin(); itr != Modules().rend(); ++itr)
    {
        (*itr)->Stop();
    }
}

std::vector<ChatCommand*> AuctionHouseModules::Commands()
{
    std::vector<ChatCommand*> tables;
    for (std::unique_ptr<AuctionHouseModule>& module : Modules())
    {
        if (ChatCommand* table = module->Commands())
        {
            tables.push_back(table);
        }
    }
    return tables;
}

uint32 AuctionHouseModules::HighestAuctionId()
{
    uint32 highest = 0;
    for (std::unique_ptr<AuctionHouseModule>& module : Modules())
    {
        highest = std::max(highest, module->HighestAuctionId());
    }
    return highest;
}

bool AuctionHouseModules::OwnsAuctionRows()
{
    for (std::unique_ptr<AuctionHouseModule>& module : Modules())
    {
        if (module->OwnsAuctionRows())
        {
            return true;
        }
    }
    return false;
}

bool AuctionHouseModules::TakesOverListings()
{
    for (std::unique_ptr<AuctionHouseModule>& module : Modules())
    {
        if (module->TakesOverListings())
        {
            return true;
        }
    }
    return false;
}

std::vector<std::unique_ptr<AuctionHouseModule>> const& AuctionHouseModules::All()
{
    return Modules();
}

void AuctionHouseModules::SetConfigFile(std::string const& path)
{
    ConfigPath() = path;
}

std::string const& AuctionHouseModules::ConfigFile()
{
    return ConfigPath();
}
