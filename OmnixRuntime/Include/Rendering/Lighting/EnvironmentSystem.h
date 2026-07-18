#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>
#include "RenderingEngine/Renderer/scene/Texture.h"
#include "Core/Engine/EngineResources.h"

namespace eng::renderer {

    struct EnvironmentMap {
        std::unique_ptr<Texture> skyboxCube;
        std::unique_ptr<Texture> irradianceCube;
        std::unique_ptr<Texture> prefilterCube;
    };

    class EnvironmentSystem {
    public:
        static EnvironmentSystem& Get() {
            static EnvironmentSystem instance;
            return instance;
        }

        EnvironmentMap* GetOrCreateEnvironment(const std::string& hdrPath, const EngineResources& res);
        Texture* GetBRDFLUT(const EngineResources& res);
        bool ProcessRawCubemap(const std::vector<std::vector<float>>& cubeFaces, uint32_t cubeSize, EnvironmentMap& outMap, const EngineResources& res);

        void Cleanup();

    private:
        EnvironmentSystem() = default;
        ~EnvironmentSystem() = default;

        std::unordered_map<std::string, std::unique_ptr<EnvironmentMap>> m_Cache;
        std::unique_ptr<Texture> m_BRDFLUT = nullptr;

        bool ProcessHDR(const std::string& hdrPath, EnvironmentMap& outMap, const EngineResources& res);
        void GenerateBRDFLUT(Texture& lut, const EngineResources& res);
    };

} // namespace eng::renderer
