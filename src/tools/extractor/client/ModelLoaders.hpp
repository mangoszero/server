#pragma once

// Turns parsed client bytes into the collision models the tile stores: a WMO's
// collidable faces folded into one soup with a BVH built over them, and an M2's
// collision hull. Both cache by path -- a WMO instanced fifty times is read once.

#include "IMpqArchive.hpp"
#include "WmoParser.hpp"
#include "stores/LiquidTypeStore.hpp"
#include "terrain/ICollisionModel.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace world::terrain
{
    class WmoLoader
    {
    public:
        WmoLoader(IMpqArchive& archive, const world::LiquidTypeStore* liquidTypes)
            : m_archive(archive), m_liquidTypes(liquidTypes) {}

        std::shared_ptr<const ICollisionModel> Load(const std::string& rootPath);

        // The furnishing tables, for baking a placement's doodad set. nullptr when the
        // WMO ships none.
        const WmoRootData* Root(const std::string& rootPath);

    private:
        IMpqArchive& m_archive;
        const world::LiquidTypeStore* m_liquidTypes;
        std::unordered_map<std::string, std::shared_ptr<const ICollisionModel>> m_cache;
        std::unordered_map<std::string, WmoRootData> m_roots;
    };

    class M2Loader
    {
    public:
        explicit M2Loader(IMpqArchive& archive) : m_archive(archive) {}

        std::shared_ptr<const ICollisionModel> Load(const std::string& path);

    private:
        IMpqArchive& m_archive;
        std::unordered_map<std::string, std::shared_ptr<const ICollisionModel>> m_cache;
    };
}
