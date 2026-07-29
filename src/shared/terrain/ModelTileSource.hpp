#pragma once

// A map whose whole terrain is one baked model, queried in the space it was baked in:
// the model's local coordinates ARE the map's, so the instance carries no placement.

#include "terrain/ICollisionModel.hpp"
#include "terrain/Terrain.hpp"

#include <memory>
#include <utility>

namespace world::terrain
{
    class ModelTileSource : public ITileSource
    {
    public:
        explicit ModelTileSource(std::shared_ptr<const ICollisionModel> model)
        {
            if (!model || model->Empty())
            {
                return;
            }

            StaticInstance inst;
            inst.worldBounds = model->Bounds();
            inst.model = std::move(model);

            auto tile = std::make_shared<TerrainTile>();
            tile->isGlobalWmo = true;
            tile->instances.push_back(std::move(inst));
            m_tile = std::move(tile);
        }

        std::shared_ptr<TerrainTile> Load(uint32_t, int, int) override { return nullptr; }

        std::shared_ptr<TerrainTile> LoadGlobal(uint32_t) override { return m_tile; }

        bool Empty() const { return m_tile == nullptr; }

    private:
        std::shared_ptr<TerrainTile> m_tile;
    };
}
