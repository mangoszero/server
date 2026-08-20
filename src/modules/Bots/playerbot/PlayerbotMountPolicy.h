#pragma once

#include "SharedDefines.h"

#include <vector>

namespace ai
{
    inline bool IsPlayerbotMountSpellCompatible(uint32 riding, bool isMountAura,
                                                 int32 castTime, int32 duration,
                                                 int32 speedIncrease)
    {
        // Classic mount rows cast for at least 500 ms, last indefinitely, and store their
        // speed increase minus one in EffectBasePoints: 59 means 60%, 99 means 100%.
        if (riding < 75 || !isMountAura || castTime < 500 || duration != -1)
        {
            return false;
        }
        return speedIncrease == (riding >= 150 ? 99 : 59);
    }

    inline void AddPlayerbotRacialMounts(std::vector<uint32>& spells, uint8 race, bool fast)
    {
        switch (race)
        {
            case RACE_HUMAN:
                if (fast)
                {
                    spells.insert(spells.end(), {23227, 23228, 23229});
                }
                else
                {
                    spells.insert(spells.end(), {458, 470, 472, 6648});
                }
                break;
            case RACE_ORC:
                if (fast)
                {
                    spells.insert(spells.end(), {23250, 23251, 23252});
                }
                else
                {
                    spells.insert(spells.end(), {580, 6653, 6654});
                }
                break;
            case RACE_DWARF:
                if (fast)
                {
                    spells.insert(spells.end(), {23238, 23239, 23240});
                }
                else
                {
                    spells.insert(spells.end(), {6777, 6898, 6899});
                }
                break;
            case RACE_NIGHTELF:
                if (fast)
                {
                    spells.insert(spells.end(), {23219, 23221, 23338});
                }
                else
                {
                    spells.insert(spells.end(), {8394, 10789, 10793});
                }
                break;
            case RACE_UNDEAD:
                if (fast)
                {
                    spells.insert(spells.end(), {17465, 23246});
                }
                else
                {
                    spells.insert(spells.end(), {17462, 17463, 17464});
                }
                break;
            case RACE_TAUREN:
                if (fast)
                {
                    spells.insert(spells.end(), {23247, 23248, 23249});
                }
                else
                {
                    spells.insert(spells.end(), {18989, 18990});
                }
                break;
            case RACE_TROLL:
                if (fast)
                {
                    spells.insert(spells.end(), {23241, 23242, 23243});
                }
                else
                {
                    spells.insert(spells.end(), {8395, 10796, 10799});
                }
                break;
            case RACE_GNOME:
                if (fast)
                {
                    spells.insert(spells.end(), {23222, 23223, 23225});
                }
                else
                {
                    spells.insert(spells.end(), {10873, 10969, 17453, 17454});
                }
                break;
        }
    }

    inline void AddPlayerbotClassMount(std::vector<uint32>& spells, uint8 race,
                                       uint8 cls, bool fast)
    {
        if (cls == CLASS_PALADIN && (race == RACE_HUMAN || race == RACE_DWARF))
        {
            spells.push_back(fast ? 23214 : 13819);
        }
        else if (cls == CLASS_WARLOCK &&
                 (race == RACE_HUMAN || race == RACE_ORC ||
                  race == RACE_UNDEAD || race == RACE_GNOME))
        {
            spells.push_back(fast ? 23161 : 5784);
        }
    }

    inline std::vector<uint32> GetPlayerbotMountSpells(uint8 race, uint8 cls, uint32 riding)
    {
        std::vector<uint32> spells;
        if (riding < 75)
        {
            return spells;
        }

        bool const fast = riding >= 150;
        AddPlayerbotRacialMounts(spells, race, fast);
        AddPlayerbotClassMount(spells, race, cls, fast);
        return spells;
    }
}
