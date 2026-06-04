#include "World.h"
#include "Core/Log/Log.h"
#include <algorithm>

namespace eng::runtime {

    World::World() {
        ENG_LOG_INFO("World created");
    }

    World::~World() {
        ENG_LOG_INFO("World destroyed");
    }

    Entity World::CreateEntity() {
        Entity e{ m_NextEntityId++ };
        m_AliveEntities.push_back(e);
        return e;
    }

    eng::core::Result World::DestroyEntity(Entity e) {
        auto it = std::find(m_AliveEntities.begin(), m_AliveEntities.end(), e);
        if (it != m_AliveEntities.end()) {
            m_AliveEntities.erase(it);
            return eng::core::Result();
        }
        return eng::core::Result(eng::core::ResultCode::Failure);
    }

    void World::RegisterSystem(SystemFn sys) {
        m_Systems.push_back(std::move(sys));
    }

    void World::Update(float deltaTime) {
        for (auto& sys : m_Systems) {
            sys(*this, deltaTime);
        }
    }

} // namespace eng::runtime
