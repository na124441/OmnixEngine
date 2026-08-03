#pragma once

namespace eng::renderer {

struct EngineResources;
class GPUScene;
class Renderer;

bool RunGPUSceneTests(EngineResources& eng, GPUScene& scene, Renderer* renderer = nullptr) noexcept;

} // namespace eng::renderer
