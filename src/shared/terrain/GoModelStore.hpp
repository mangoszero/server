#pragma once

// Game-object collision models, by GameObjectDisplayInfo.dbc id.
//
// One model is shared by every game object of that display id -- a keep's gate appears
// hundreds of times and its geometry is held once. A miss is remembered as a null so a
// display id with no collision costs one failed open, not one per spawn.

#include <string>
#include "terrain/ICollisionModel.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace world::terrain
{
    class GoModelStore
    {
    public:
        static GoModelStore& Instance();

        // Directory the baked go_<displayId>.tile files live in. Set once at startup.
        void SetDirectory(const std::string& dir);

        // nullptr when the display id has no collision geometry.
        std::shared_ptr<const ICollisionModel> Get(uint32_t displayId);

        void Clear();

    private:
        GoModelStore() = default;

        std::string m_dir;
        std::unordered_map<uint32_t, std::shared_ptr<const ICollisionModel>> m_models;
        std::mutex m_mutex;
    };
}
