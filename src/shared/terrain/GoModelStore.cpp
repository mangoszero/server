#include <memory>
#include <string>
#include <mutex>
#include "terrain/GoModelStore.hpp"
#include "terrain/TileSerializer.hpp"

namespace world::terrain
{
    GoModelStore& GoModelStore::Instance()
    {
        static GoModelStore store;
        return store;
    }

    void GoModelStore::SetDirectory(const std::string& dir)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_dir = dir;
        m_models.clear();
    }

    void GoModelStore::Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_models.clear();
    }

    std::shared_ptr<const ICollisionModel> GoModelStore::Get(uint32_t displayId)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto found = m_models.find(displayId);
        if (found != m_models.end())
        {
            return found->second;
        }

        std::shared_ptr<const ICollisionModel> model;
        if (!m_dir.empty())
        {
            if (auto tile = ReadTile(m_dir + "/" + GoModelFileName(displayId)))
            {
                if (!tile->instances.empty())
                {
                    model = tile->instances.front().model;
                }
            }
        }

        // A null is cached too: it records that this display id has no collision, which
        // is true of most of them, and spares a failed open per spawn.
        m_models.emplace(displayId, model);
        return model;
    }
}
