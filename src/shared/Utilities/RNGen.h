/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_RNG_H
#define MANGOS_RNG_H

#include <random>

#include "ace/Singleton.h"
#include "ace/Synch_Traits.h"
#include "Platform/Define.h"

class RNGen
{
public:
    RNGen()
    {
        std::random_device rd;
        gen_.seed(rd());
    }

    int32 rand_i(int32 min, int32 max)
    {
        std::uniform_int_distribution<int32> dist{min, max};
        return dist(gen_);
    }

    uint32 rand_u(uint32 min, uint32 max)
    {
        std::uniform_int_distribution<uint32> dist{min, max};
        return dist(gen_);
    }

    uint32 rand()
    {
        std::uniform_int_distribution<uint32> dist;
        return dist(gen_);
    }

    float rand_f(float min, float max)
    {
        std::uniform_real_distribution<float> dist{min, max};
        return dist(gen_);
    }

    double rand_d(double min, double max)
    {
        std::uniform_real_distribution<double> dist{min, max};
        return dist(gen_);
    }

private:
    std::mt19937 gen_;
};

typedef ACE_TSS_Singleton<RNGen, ACE_SYNCH_MUTEX> RNG;

#endif