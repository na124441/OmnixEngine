#include "Runtime/HotReloadTests.h"
#include "Runtime/AssetManager.h"
#include "Runtime/AssetRegistry.h"
#include "Runtime/HotReloadSystem.h"
#include "Core/Logging/Logger.h"
#include <fstream>
#include <filesystem>
#include <cstdlib>

namespace eng::runtime {

    namespace {
        class MockShaderLoader : public IAssetLoader
        {
        public:
            MockShaderLoader() = default;
            AssetType GetSupportedType() const override { return AssetType::Shader; }
            bool CanLoad(AssetType type) const override { return type == AssetType::Shader; }

            bool Load(const AssetMetadata& metadata, RuntimeAsset** outAsset) override
            {
                *outAsset = new RuntimeAsset();
                m_LoadCount++;
                return true;
            }

            void Unload(RuntimeAsset* asset) override
            {
                delete asset;
                m_UnloadCount++;
            }

            uint32_t GetLoadCount() const { return m_LoadCount; }

        private:
            uint32_t m_LoadCount = 0;
            uint32_t m_UnloadCount = 0;
        };

        bool CheckGlslc()
        {
            std::string glslcCmd = "glslc";
            const char* vulkanSdk = std::getenv("VULKAN_SDK");
            if (vulkanSdk) {
                glslcCmd = std::string("\"") + vulkanSdk + "/bin/glslc.exe\"";
            }
            std::string command = glslcCmd + " --version >nul 2>&1";
            int code = std::system(command.c_str());
            return (code == 0);
        }
    }

    bool RunShaderReloadTests() noexcept
    {
        LOG_INFO("[ShaderReloadTest] Running shader reload tests...");

        AssetRegistry registry;
        AssetManager manager(registry);

        auto loaderUnique = std::make_unique<MockShaderLoader>();
        manager.RegisterLoader(AssetType::Shader, std::move(loaderUnique));

        AssetHandle handle{3001};
        AssetMetadata meta;
        meta.handle = handle;
        meta.type = AssetType::Shader;
        meta.sourcePath = "TempShader.frag";
        meta.importedPath = "TempShader.spv";
        meta.isImported = true;
        registry.UpdateMetadata(meta);

        // Pre-create initial asset in cache
        manager.LoadAsset(handle);

        HotReloadSystem hotReload(registry, manager);

        bool glslcAvailable = CheckGlslc();
        if (!glslcAvailable) {
            LOG_WARN("[ShaderReloadTest] glslc is not available on PATH/VULKAN_SDK. Skipping physical compilation tests.");
            LOG_INFO("[ShaderReloadTest] Running mock reload validation instead...");

            // Test mock shader reload triggers
            bool success = hotReload.ReloadAsset(handle);
            if (!success) {
                LOG_ERROR("[ShaderReloadTest] Mock reload failed!");
                return false;
            }
            LOG_INFO("[ShaderReloadTest] Shader reload tests passed (mock mode).");
            return true;
        }

        // Test 1: Valid Shader compile & reload
        {
            // Write a simple valid fragment shader
            std::ofstream shaderFile(meta.sourcePath);
            shaderFile << "#version 450\n"
                          "layout(location = 0) out vec4 outColor;\n"
                          "void main() {\n"
                          "    outColor = vec4(1.0, 0.0, 0.0, 1.0);\n"
                          "}\n";
            shaderFile.close();

            bool success = hotReload.ReloadAsset(handle);
            if (!success) {
                LOG_ERROR("[ShaderReloadTest] Valid shader reload failed!");
                std::filesystem::remove(meta.sourcePath);
                std::filesystem::remove(meta.importedPath);
                return false;
            }

            if (!std::filesystem::exists(meta.importedPath)) {
                LOG_ERROR("[ShaderReloadTest] Output SPIR-V file was not generated!");
                std::filesystem::remove(meta.sourcePath);
                return false;
            }
            std::filesystem::remove(meta.importedPath);
        }

        // Test 2: Invalid Shader compilation failure (safety swap check)
        {
            // Write invalid syntax shader
            std::ofstream shaderFile(meta.sourcePath);
            shaderFile << "#version 450\n"
                          "void main() {\n"
                          "    invalid_syntax_error;\n"
                          "}\n";
            shaderFile.close();

            // Reload should fail gracefully and not replace the cached asset
            bool success = hotReload.ReloadAsset(handle);
            if (success) {
                LOG_ERROR("[ShaderReloadTest] ReloadAsset reported success for compile error!");
                std::filesystem::remove(meta.sourcePath);
                return false;
            }

            // Diagnostic event must contain compile errors in history log
            const auto& history = hotReload.GetHistory();
            if (history.empty() || history.back().state != ReloadState::Failed) {
                LOG_ERROR("[ShaderReloadTest] State was not marked as Failed!");
                std::filesystem::remove(meta.sourcePath);
                return false;
            }

            std::filesystem::remove(meta.sourcePath);
        }

        LOG_INFO("[ShaderReloadTest] All shader compilation reload tests passed.");
        return true;
    }

} // namespace eng::runtime
