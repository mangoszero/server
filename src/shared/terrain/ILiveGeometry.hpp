#pragma once

// Geometry the server poses at runtime -- a door that swings, a lift that rises -- which
// no baked tile can hold. Declared on this side so the terrain engine can COMPOSE it into
// a column without naming a game type: the implementation lives in the game, and `filter`
// is an opaque token this side carries and never interprets.

#include "terrain/Column.hpp"

#include <cstdint>

namespace world::terrain
{
    class ILiveGeometry
    {
        public:
            virtual ~ILiveGeometry() = default;

            virtual void AddSurfaces(float x, float y, float zTop, float zBottom,
                                     uint32_t filter, Column& out) const = 0;
    };
}
