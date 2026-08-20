#pragma once

#include "SharedDefines.h"

#include <map>
#include <vector>

namespace ai
{
    inline std::vector<uint8> const* FindRandomBotRaceCandidates(
        std::map<uint8, std::vector<uint8> > const& candidates, uint8 cls)
    {
        std::map<uint8, std::vector<uint8> >::const_iterator itr = candidates.find(cls);
        if (itr == candidates.end() || itr->second.empty())
        {
            return nullptr;
        }
        return &itr->second;
    }

    inline std::vector<uint8> GetMissingRandomBotClasses(std::vector<uint8> const& existing)
    {
        bool present[MAX_CLASSES] = {};
        for (std::vector<uint8>::const_iterator itr = existing.begin(); itr != existing.end(); ++itr)
        {
            if (*itr < MAX_CLASSES)
            {
                present[*itr] = true;
            }
        }

        uint8 const playable[] = {
            CLASS_WARRIOR,
            CLASS_PALADIN,
            CLASS_HUNTER,
            CLASS_ROGUE,
            CLASS_PRIEST,
            CLASS_SHAMAN,
            CLASS_MAGE,
            CLASS_WARLOCK,
            CLASS_DRUID
        };

        std::vector<uint8> missing;
        for (uint8 cls : playable)
        {
            if (!present[cls])
            {
                missing.push_back(cls);
            }
        }
        return missing;
    }
}
