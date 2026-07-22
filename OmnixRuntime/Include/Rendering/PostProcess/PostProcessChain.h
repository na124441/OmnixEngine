#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace eng::renderer {

    enum class PostProcessPassType {
        SSAO,
        Bloom,
        HDRTonemap,
        ColorGrading
    };

    struct PostProcessSettings {
        bool enableSSAO = true;
        bool enableBloom = true;
        bool enableTonemap = true;
        bool enableColorGrading = true;
        float exposure = 1.0f;
        float bloomIntensity = 0.5f;
        float ssaoRadius = 0.5f;
    };

    class PostProcessChain {
    public:
        PostProcessChain() = default;
        ~PostProcessChain() = default;

        void Initialize(const PostProcessSettings& settings = {}) {
            m_Settings = settings;
            RebuildChain();
        }

        void SetSettings(const PostProcessSettings& settings) {
            m_Settings = settings;
            RebuildChain();
        }

        const PostProcessSettings& GetSettings() const { return m_Settings; }
        const std::vector<PostProcessPassType>& GetActivePasses() const { return m_ActivePasses; }

        void ExecuteChain(VkCommandBuffer cmd) {
            for (auto pass : m_ActivePasses) {
                switch (pass) {
                case PostProcessPassType::SSAO:
                    // Record SSAO pass
                    break;
                case PostProcessPassType::Bloom:
                    // Record Bloom pass
                    break;
                case PostProcessPassType::HDRTonemap:
                    // Record Tonemap pass
                    break;
                case PostProcessPassType::ColorGrading:
                    // Record Color Grading pass
                    break;
                }
            }
        }

    private:
        void RebuildChain() {
            m_ActivePasses.clear();
            if (m_Settings.enableSSAO) m_ActivePasses.push_back(PostProcessPassType::SSAO);
            if (m_Settings.enableBloom) m_ActivePasses.push_back(PostProcessPassType::Bloom);
            if (m_Settings.enableTonemap) m_ActivePasses.push_back(PostProcessPassType::HDRTonemap);
            if (m_Settings.enableColorGrading) m_ActivePasses.push_back(PostProcessPassType::ColorGrading);
        }

        PostProcessSettings m_Settings;
        std::vector<PostProcessPassType> m_ActivePasses;
    };

} // namespace eng::renderer
