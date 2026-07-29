#pragma once

// LiquidType.dbc -> { id : (category, spellId) }.
//
// In 3.3.5a the `Type` column is authoritative and reads 0 = water, 1 = ocean,
// 2 = magma, 3 = slime. That is NOT the 2.4.3 encoding (0 = magma, 2 = slime,
// 3 = water, ocean indistinguishable), which is why the 2.4.3 extractors classify
// rows below 21 by id arithmetic instead. Here the column answers directly.

#include "MpqDbcLoader.hpp"
#include "terrain/Terrain.hpp"

#include "Server/DBCfmt.h"

#include <cstdint>
#include <unordered_map>

namespace world
{
    enum class LiquidDbcType : uint32_t
    {
        Water = 0,
        Ocean = 1,
        Magma = 2,
        Slime = 3
    };

    struct LiquidTypeInfo
    {
        uint32_t type = 0;     ///< the `Type` column; see LiquidDbcType
        uint32_t spellId = 0;  ///< aura applied while in this liquid
    };

    class LiquidTypeStore
    {
    public:
        bool LoadFromDbc(world::terrain::IMpqArchive& archive)
        {
            DBCFileLoader dbc;
            if (!LoadDbcFromMpq(archive, "DBFilesClient\\LiquidType.dbc", LiquidTypefmt, dbc))
            {
                return false;
            }

            m_entries.clear();
            for (uint32_t r = 0; r < dbc.GetNumRows(); ++r)
            {
                DBCFileLoader::Record rec = dbc.getRecord(r);
                LiquidTypeInfo info;
                info.type = rec.getUInt(3);
                info.spellId = rec.getUInt(5);
                m_entries[rec.getUInt(0)] = info;
            }
            return true;
        }

        const LiquidTypeInfo* Find(uint32_t id) const
        {
            auto it = m_entries.find(id);
            return it != m_entries.end() ? &it->second : nullptr;
        }

        size_t Size() const { return m_entries.size(); }

    private:
        std::unordered_map<uint32_t, LiquidTypeInfo> m_entries;
    };

    // Category of a LiquidType.dbc row. The store is the authority; the fallback table
    // covers only the canonical rows, for a bake that has no client to hand (a stand-alone
    // game-object model). Its values are the 3.3.5a file's own, not a guess.
    inline world::terrain::LiquidKind ClassifyLiquid(uint32_t entry,
                                                     const LiquidTypeStore* store)
    {
        using world::terrain::LiquidKind;
        if (!entry)
        {
            return LiquidKind::None;
        }

        if (store)
        {
            if (const LiquidTypeInfo* info = store->Find(entry))
            {
                switch (static_cast<LiquidDbcType>(info->type))
                {
                    case LiquidDbcType::Water: return LiquidKind::Water;
                    case LiquidDbcType::Ocean: return LiquidKind::Ocean;
                    case LiquidDbcType::Magma: return LiquidKind::Magma;
                    case LiquidDbcType::Slime: return LiquidKind::Slime;
                }
            }
        }

        if (entry <= 12)
        {
            switch ((entry - 1) & 3)
            {
                case 0: return LiquidKind::Water;
                case 1: return LiquidKind::Ocean;
                case 2: return LiquidKind::Magma;
                default: return LiquidKind::Slime;
            }
        }
        switch (entry)
        {
            case 13: case 17: case 41: case 61: case 81: return LiquidKind::Water;
            case 14: case 100: return LiquidKind::Ocean;
            case 15: case 19: case 121: case 141: return LiquidKind::Magma;
            case 20: case 21: case 181: return LiquidKind::Slime;
            default: return LiquidKind::Water;
        }
    }
}
