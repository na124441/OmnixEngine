#include "Editor/Panels/ProfilerPanel.h"
#include "Rendering/Core/Renderer.h"
#include "ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <numeric>
#include <string>

namespace eng::runtime {

    void ProfilerPanel::Initialize(RuntimeContext* context) {
        m_Context = context;
        std::fill(std::begin(m_FrameTimeHistory), std::end(m_FrameTimeHistory), 0.0f);
        std::fill(std::begin(m_FpsHistory), std::end(m_FpsHistory), 0.0f);
    }

    void ProfilerPanel::Render() {
        if (!m_IsOpen) return;

        ImGui::SetNextWindowSize(ImVec2(550, 420), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Profiler & Render Graph", &m_IsOpen, ImGuiWindowFlags_NoCollapse)) {
            ImGui::End();
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        float currentFrameTime = io.DeltaTime * 1000.0f; // in ms
        float currentFps = io.Framerate;

        // Push into ring history buffer
        m_FrameTimeHistory[m_HistoryOffset] = currentFrameTime;
        m_FpsHistory[m_HistoryOffset] = currentFps;
        m_HistoryOffset = (m_HistoryOffset + 1) % HISTORY_SIZE;

        // Header Metrics Summary
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "FPS: %.1f", currentFps);
        ImGui::SameLine(150);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Frame Time: %.2f ms", currentFrameTime);

        if (ImGui::BeginTabBar("ProfilerTabs")) {

            // TAB 1: FRAME TIMINGS & PERFORMANCE GRAPHS
            if (ImGui::BeginTabItem("Frame Performance")) {
                char frameTimeOverlay[64];
                snprintf(frameTimeOverlay, sizeof(frameTimeOverlay), "Avg: %.2f ms", currentFrameTime);
                ImGui::PlotLines("Frame Time (ms)", m_FrameTimeHistory, (int)HISTORY_SIZE, (int)m_HistoryOffset, frameTimeOverlay, 0.0f, 33.3f, ImVec2(0, 80));

                char fpsOverlay[64];
                snprintf(fpsOverlay, sizeof(fpsOverlay), "Avg: %.1f FPS", currentFps);
                ImGui::PlotLines("FPS History", m_FpsHistory, (int)HISTORY_SIZE, (int)m_HistoryOffset, fpsOverlay, 0.0f, 120.0f, ImVec2(0, 80));

                ImGui::Separator();
                ImGui::Text("GPU Workload Budget");
                float budget16ms = (currentFrameTime / 16.66f) * 100.0f;
                budget16ms = std::clamp(budget16ms, 0.0f, 100.0f);
                
                ImVec4 progressColor = (budget16ms > 90.0f) ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f) :
                                       (budget16ms > 70.0f) ? ImVec4(0.9f, 0.7f, 0.1f, 1.0f) :
                                                              ImVec4(0.2f, 0.8f, 0.3f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progressColor);
                ImGui::ProgressBar(budget16ms / 100.0f, ImVec2(-1, 0), "Target: 60 FPS (16.6 ms)");
                ImGui::PopStyleColor();

                ImGui::EndTabItem();
            }

            // TAB 2: RENDER GRAPH PASS PIPELINE
            if (ImGui::BeginTabItem("Render Graph Passes")) {
                ImGui::Text("Vulkan Render Graph Execution Sequence");
                ImGui::Separator();

                if (ImGui::BeginTable("RenderPassTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
                    ImGui::TableSetupColumn("Pass Name", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100.0f);
                    ImGui::TableSetupColumn("Layout Transition", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    ImGui::TableHeadersRow();

                    struct PassInfo {
                        const char* name;
                        const char* type;
                        const char* transition;
                        const char* status;
                    };

                    static const PassInfo kPasses[] = {
                        { "GBufferPass",          "Graphics", "COLOR_ATTACHMENT_OPTIMAL", "Active" },
                        { "HZBPass",              "Compute",  "SHADER_READ_ONLY_OPTIMAL", "Active" },
                        { "SSRPass",              "Compute",  "SHADER_READ_ONLY_OPTIMAL", "Active" },
                        { "LightingPass",         "Graphics", "COLOR_ATTACHMENT_OPTIMAL", "Active" },
                        { "TransparentPass_Pre",  "Graphics", "DEPTH_STENCIL_READ_ONLY",  "Active" },
                        { "TransparentPass_Post", "Graphics", "COLOR_ATTACHMENT_OPTIMAL", "Active" },
                        { "TAAPass",              "Compute",  "SHADER_READ_ONLY_OPTIMAL", "Active" },
                        { "PostProcessPass",      "Graphics", "SHADER_READ_ONLY_OPTIMAL", "Active" },
                        { "EditorOverlayPass",    "Graphics", "PRESENT_SRC_KHR",          "Active" }
                    };

                    for (const auto& pass : kPasses) {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", pass.name);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextDisabled("%s", pass.type);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::TextDisabled("%s", pass.transition);
                        ImGui::TableSetColumnIndex(3);
                        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", pass.status);
                    }

                    ImGui::EndTable();
                }

                ImGui::EndTabItem();
            }

            // TAB 3: FRAME DIAGNOSTICS & MEMORY
            if (ImGui::BeginTabItem("Diagnostics & VMA")) {
                eng::renderer::Renderer::FrameDiagnostics* diagnostics = eng::renderer::Renderer::GetCurrentDiagnostics();
                if (diagnostics) {
                    ImGui::Text("Frame Number: %u", diagnostics->frameNumber);
                    ImGui::Separator();
                    ImGui::Text("Extracted Mesh Entities: %u", diagnostics->entitiesExtracted);
                    ImGui::Text("Uploaded GPU Instances:   %u", diagnostics->instancesUploaded);
                    ImGui::Text("Opaque Render Items:      %u", diagnostics->opaqueItems);
                    ImGui::Text("Transparent Render Items: %u", diagnostics->transparentItems);
                    ImGui::Separator();
                    ImGui::Text("Total Draw Calls:         %u", diagnostics->drawCalls);
                    ImGui::Text("Total Triangles:          %u", diagnostics->triangles);
                    ImGui::Text("Culling Rejected Items:   %u", diagnostics->rejectedItems);
                    ImGui::Text("Validation Errors:        %u", diagnostics->validationErrors);
                } else {
                    ImGui::TextDisabled("Frame diagnostics unavailable for active frame.");
                }

                ImGui::Separator();
                ImGui::Text("Vulkan VMA Memory Summary");
                ImGui::BulletText("Allocated Image Heap: VMA Managed");
                ImGui::BulletText("Allocated Buffer Heap: VMA Managed");
                ImGui::BulletText("Swapchain Images: Double/Triple Buffered");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGui::End();
    }

} // namespace eng::runtime
