#pragma once

// Bridges the client MPQ to the server's own DBC reader. The baker carries no DBC
// parser of its own: it reads the raw WDBC image out of the archive and hands it to
// DBCFileLoader, driven by the format strings in Server/DBCfmt.h. One parser and one
// column layout, so the baker cannot drift from the server's view of a .dbc.
//
// The server marks columns it does not care about as 'x', and Map.dbc's Directory is
// one of them -- yet that directory is exactly what the baker needs for tile paths.
// That is fine: 'x' and 's' are both four bytes wide and neither of the files read
// here contains a byte field, so every offset DBCFileLoader computes is unchanged and
// getString reads the column regardless of its format char.

#include "IMpqArchive.hpp"

#include "DataStores/DBCFileLoader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace world
{
    inline bool LoadDbcFromMpq(world::terrain::IMpqArchive& archive,
                               const std::string& dbcPath, const char* fmt,
                               DBCFileLoader& out)
    {
        std::vector<uint8_t> bytes;
        if (!archive.Read(dbcPath, bytes))
        {
            return false;
        }
        return out.LoadFromMemory(bytes.data(), bytes.size(), fmt);
    }
}
