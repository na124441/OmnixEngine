#include "SceneBuilder.h"
#include "Core/Log/Log.h"

namespace eng::runtime {

    SceneBuilder::SceneBuilder(AssetCache* assetCache) : m_AssetCache(assetCache) {
        ENG_LOG_INFO("SceneBuilder created");
    }

} // namespace eng::runtime
