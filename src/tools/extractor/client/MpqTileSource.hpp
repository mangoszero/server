#pragma once

// Assembles one TerrainTile from the client: the ADT's height/liquid grids plus every
// WMO and M2 placed on it, each as a shared model and its own placement Transform.
//
// Tile indexing throughout: tx is the tile index derived from world X, ty from world Y.
// The ADT file is named the other way round -- "<Map>_<ty>_<tx>.adt".

#include "IMpqArchive.hpp"
#include "ModelLoaders.hpp"
#include "WdtParser.hpp"
#include "stores/MapDbcStore.hpp"
#include "terrain/Terrain.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace world::terrain
{
    class MpqTileSource : public ITileSource
    {
    public:
        MpqTileSource(IMpqArchive& archive, const world::MapDbcStore* maps,
                      const world::LiquidTypeStore* liquidTypes)
            : m_archive(archive), m_maps(maps), m_liquidTypes(liquidTypes),
              m_wmo(archive, liquidTypes), m_m2(archive) {}

        std::shared_ptr<TerrainTile> Load(uint32_t mapId, int tx, int ty) override;

        void SetLoadStatics(bool on) { m_loadStatics = on; }

        std::string MapDirectory(uint32_t mapId) const;
        std::string AdtPath(uint32_t mapId, int tx, int ty) const;
        std::string WdtPath(uint32_t mapId) const;

        // Parsed once per map; the tile bake walks it to know which tiles exist.
        const WdtData* Wdt(uint32_t mapId);

    private:
        std::shared_ptr<TerrainTile> LoadAdt(uint32_t mapId, int tx, int ty);
        std::shared_ptr<TerrainTile> LoadGlobalWmo(uint32_t mapId);
        void AttachWmoDoodads(const Placement& p, const std::string& wmoPath,
                              const Transform& wmoXf, TerrainTile& tile);

        IMpqArchive& m_archive;
        const world::MapDbcStore* m_maps;
        const world::LiquidTypeStore* m_liquidTypes;
        WmoLoader m_wmo;
        M2Loader m_m2;
        bool m_loadStatics = true;

        std::unordered_map<uint32_t, WdtData> m_wdtCache;
        std::unordered_map<uint32_t, std::shared_ptr<TerrainTile>> m_globalWmoCache;
    };
}
