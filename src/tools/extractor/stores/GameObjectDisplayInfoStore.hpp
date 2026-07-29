#pragma once

// GameObjectDisplayInfo.dbc -> { displayId : model path }. The names are the .mdx/.mdl
// the client ships; the file on disk is .m2 for models and .wmo for buildings.

#include "MpqDbcLoader.hpp"

#include "Server/DBCfmt.h"

#include <cctype>
#include <cstdint>
#include <map>
#include <string>

namespace world
{
    class GameObjectDisplayInfoStore
    {
    public:
        bool LoadFromDbc(world::terrain::IMpqArchive& archive)
        {
            DBCFileLoader dbc;
            if (!LoadDbcFromMpq(archive, "DBFilesClient\\GameObjectDisplayInfo.dbc",
                                GameObjectDisplayInfofmt, dbc))
            {
                return false;
            }

            m_models.clear();
            for (uint32_t r = 0; r < dbc.GetNumRows(); ++r)
            {
                DBCFileLoader::Record rec = dbc.getRecord(r);
                const char* name = rec.getString(1);
                if (name && *name)
                {
                    m_models[rec.getUInt(0)] = name;
                }
            }
            return true;
        }

        const std::map<uint32_t, std::string>& All() const { return m_models; }

        static bool IsWmo(const std::string& path)
        {
            return path.size() >= 4 &&
                   std::tolower(static_cast<unsigned char>(path[path.size() - 3])) == 'w' &&
                   std::tolower(static_cast<unsigned char>(path[path.size() - 2])) == 'm' &&
                   std::tolower(static_cast<unsigned char>(path[path.size() - 1])) == 'o';
        }

    private:
        std::map<uint32_t, std::string> m_models;
    };
}
