#include "Core/pch.h"
#include "RenderGraph.h"
#include "Core/Vulkan/VkUtils.h"
#include "Core/Engine/Log.h"
#include <algorithm>
#include <cstring>

namespace eng::renderer {

void RenderGraph::RegisterPass(
    const std::string& name,
    const std::vector<std::string>& inputs,
    const std::vector<std::string>& outputs,
    PassID physicalSlot,
    std::function<void(VkCommandBuffer)> execute
)
{
    RenderPass pass{};
    pass.name = name;
    pass.inputs = inputs;
    pass.outputs = outputs;
    pass.physicalSlot = physicalSlot;
    pass.execute = execute;

    m_RegisteredPasses.push_back(pass);
}

void RenderGraph::DeclareTexture(const std::string& name, const TextureResourceDesc& desc, bool transient)
{
    RenderResource res{};
    res.name = name;
    res.type = RenderResourceType::Texture;
    res.isTransient = transient;
    res.textureDesc = desc;

    m_Resources[name] = res;
}

void RenderGraph::DeclareBuffer(const std::string& name, const BufferResourceDesc& desc, bool transient)
{
    RenderResource res{};
    res.name = name;
    res.type = RenderResourceType::Buffer;
    res.isTransient = transient;
    res.bufferDesc = desc;

    m_Resources[name] = res;
}

void RenderGraph::Compile(EngineResources& resources)
{
    // 1. Sort passes topologically based on read/write dependencies
    TopologicalSort();

    // 2. Calculate resource first & last pass lifetimes
    CalculateResourceLifetimes();

    EnsureTimestampQueries(resources);
}

void RenderGraph::EnsureTimestampQueries(EngineResources& resources)
{
    if (resources.device == VK_NULL_HANDLE || resources.physicalDevice == VK_NULL_HANDLE) {
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(resources.physicalDevice, &props);
    m_TimestampPeriodNs = props.limits.timestampPeriod;

    uint32_t requiredQueries = static_cast<uint32_t>(m_CompiledPasses.size() * 2);
    if (requiredQueries == 0 || (m_TimestampQueryPool != VK_NULL_HANDLE && m_TimestampQueryCapacity >= requiredQueries)) {
        return;
    }

    if (m_TimestampQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(resources.device, m_TimestampQueryPool, nullptr);
        m_TimestampQueryPool = VK_NULL_HANDLE;
        m_TimestampQueryCapacity = 0;
    }

    VkQueryPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    poolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    poolInfo.queryCount = requiredQueries;
    if (vkCreateQueryPool(resources.device, &poolInfo, nullptr, &m_TimestampQueryPool) == VK_SUCCESS) {
        m_TimestampQueryCapacity = requiredQueries;
    }
}

void RenderGraph::CollectGpuTimings(EngineResources& resources, uint32_t frameIndex)
{
    (void)frameIndex;
    m_LastPassTimings.clear();
    m_LastGpuFrameTimeMs = 0.0f;

    if (resources.device == VK_NULL_HANDLE || m_TimestampQueryPool == VK_NULL_HANDLE || m_LastTimingNames.empty()) {
        return;
    }

    const uint32_t queryCount = static_cast<uint32_t>(m_LastTimingNames.size() * 2);
    if (queryCount == 0 || queryCount > m_TimestampQueryCapacity) {
        return;
    }

    std::vector<uint64_t> timestamps(queryCount, 0);
    VkResult result = vkGetQueryPoolResults(resources.device,
                                            m_TimestampQueryPool,
                                            0,
                                            queryCount,
                                            sizeof(uint64_t) * timestamps.size(),
                                            timestamps.data(),
                                            sizeof(uint64_t),
                                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    if (result != VK_SUCCESS) {
        return;
    }

    m_LastPassTimings.reserve(m_LastTimingNames.size());
    for (size_t i = 0; i < m_LastTimingNames.size(); ++i) {
        uint64_t begin = timestamps[i * 2];
        uint64_t end = timestamps[i * 2 + 1];
        float ms = 0.0f;
        if (end >= begin) {
            ms = static_cast<float>(static_cast<double>(end - begin) * static_cast<double>(m_TimestampPeriodNs) / 1000000.0);
        }
        m_LastGpuFrameTimeMs += ms;
        m_LastPassTimings.push_back(RenderPassTiming{m_LastTimingNames[i], ms});
    }
}

void RenderGraph::Execute(EngineResources& resources, uint32_t frameIndex)
{
    EnsureTimestampQueries(resources);
    m_LastTimingNames.clear();

    // Resolve Vulkan debug labels extension function pointers dynamically
    PFN_vkCmdBeginDebugUtilsLabelEXT pfnBeginDebugLabel = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT pfnEndDebugLabel = nullptr;

    if (resources.device != VK_NULL_HANDLE) {
        pfnBeginDebugLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(resources.device, "vkCmdBeginDebugUtilsLabelEXT");
        pfnEndDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(resources.device, "vkCmdEndDebugUtilsLabelEXT");
    }

    if (pfnBeginDebugLabel == nullptr && resources.instance != VK_NULL_HANDLE) {
        pfnBeginDebugLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(resources.instance, "vkCmdBeginDebugUtilsLabelEXT");
        pfnEndDebugLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(resources.instance, "vkCmdEndDebugUtilsLabelEXT");
    }

    // Keep track of which physical command buffers have been begun
    std::vector<bool> cmdBufferBegun(PASS_COUNT, false);
    bool timestampQueriesReset = false;
    uint32_t queryIndex = 0;

    for (const auto& pass : m_CompiledPasses) {
        uint32_t slotIdx = static_cast<uint32_t>(pass.physicalSlot);
        if (slotIdx >= PASS_COUNT) {
            LOG_ERROR("RenderGraph: Pass \"" + pass.name + "\" has invalid physical slot: " + std::to_string(slotIdx));
            continue;
        }

        VkCommandBuffer cmd = resources.commandBuffers[frameIndex][slotIdx];

        if (!cmdBufferBegun[slotIdx]) {
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
            cmdBufferBegun[slotIdx] = true;
        }

        bool useTimestamp = m_TimestampQueryPool != VK_NULL_HANDLE && queryIndex + 1 < m_TimestampQueryCapacity;
        if (useTimestamp && !timestampQueriesReset) {
            vkCmdResetQueryPool(cmd, m_TimestampQueryPool, 0, m_TimestampQueryCapacity);
            timestampQueriesReset = true;
        }

        if (useTimestamp) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_TimestampQueryPool, queryIndex);
            m_LastTimingNames.push_back(pass.name);
        }

        // Insert Debug Label marker around pass execution if available
        if (pfnBeginDebugLabel != nullptr) {
            VkDebugUtilsLabelEXT label{};
            label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            label.pLabelName = pass.name.c_str();
            float color[4] = { 0.18f, 0.54f, 0.34f, 1.0f }; // Nice green-ish theme
            std::memcpy(label.color, color, sizeof(color));
            pfnBeginDebugLabel(cmd, &label);
        }

        // Execute pass lambda recording
        if (pass.execute) {
            pass.execute(cmd);
        }

        if (useTimestamp) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_TimestampQueryPool, queryIndex + 1);
            queryIndex += 2;
        }

        if (pfnEndDebugLabel != nullptr) {
            pfnEndDebugLabel(cmd);
        }
    }

    // End all command buffers that were begun
    for (uint32_t i = 0; i < PASS_COUNT; ++i) {
        if (cmdBufferBegun[i]) {
            VkCommandBuffer cmd = resources.commandBuffers[frameIndex][i];
            VK_CHECK(vkEndCommandBuffer(cmd));
        }
    }
}

void RenderGraph::PrintDebug() const
{
    LOG_INFO("================= Render Graph Debug Dump =================");
    LOG_INFO("Execution Order (" + std::to_string(m_CompiledPasses.size()) + " passes):");
    for (size_t i = 0; i < m_CompiledPasses.size(); ++i) {
        const auto& pass = m_CompiledPasses[i];
        std::string inputsStr = "";
        for (const auto& in : pass.inputs) {
            inputsStr += (inputsStr.empty() ? "" : ", ") + in;
        }
        std::string outputsStr = "";
        for (const auto& out : pass.outputs) {
            outputsStr += (outputsStr.empty() ? "" : ", ") + out;
        }

        LOG_INFO("  [" + std::to_string(i) + "] Pass: \"" + pass.name + "\"");
        LOG_INFO("      Physical Slot: " + std::to_string(static_cast<uint32_t>(pass.physicalSlot)));
        LOG_INFO("      Inputs:        [" + inputsStr + "]");
        LOG_INFO("      Outputs:       [" + outputsStr + "]");
    }

    LOG_INFO("Resource Lifetimes:");
    for (const auto& pair : m_Resources) {
        const auto& res = pair.second;
        if (res.firstPass != 0xFFFFFFFF) {
            LOG_INFO("  Resource: \"" + res.name + "\" | First Pass: " + std::to_string(res.firstPass) + " | Last Pass: " + std::to_string(res.lastPass));
        } else {
            LOG_INFO("  Resource: \"" + res.name + "\" | Unused");
        }
    }
    LOG_INFO("===========================================================");
}

void RenderGraph::Clear()
{
    m_RegisteredPasses.clear();
    m_Resources.clear();
    m_CompiledPasses.clear();
    m_LastTimingNames.clear();
}

void RenderGraph::Shutdown(EngineResources& resources)
{
    if (resources.device != VK_NULL_HANDLE && m_TimestampQueryPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(resources.device, m_TimestampQueryPool, nullptr);
    }
    m_TimestampQueryPool = VK_NULL_HANDLE;
    m_TimestampQueryCapacity = 0;
    Clear();
    m_LastPassTimings.clear();
    m_LastGpuFrameTimeMs = 0.0f;
}

bool RenderGraph::TopologicalSort()
{
    size_t n = m_RegisteredPasses.size();
    std::vector<std::vector<size_t>> adj(n);
    std::vector<size_t> inDegree(n, 0);

    for (size_t i = 0; i < n; ++i) {
        const auto& passA = m_RegisteredPasses[i];
        for (size_t j = 0; j < n; ++j) {
            if (i == j) continue;
            const auto& passB = m_RegisteredPasses[j];

            bool precedes = false;

            // passA writes to a resource passB reads (RAW)
            for (const auto& outA : passA.outputs) {
                for (const auto& inB : passB.inputs) {
                    if (outA == inB) { precedes = true; break; }
                }
                if (precedes) break;
                
                // passA writes to a resource passB writes (WAW) and i < j
                for (const auto& outB : passB.outputs) {
                    if (outA == outB && i < j) { precedes = true; break; }
                }
                if (precedes) break;
            }

            if (!precedes && i < j) {
                // passA reads a resource passB writes (WAR)
                for (const auto& inA : passA.inputs) {
                    for (const auto& outB : passB.outputs) {
                        if (inA == outB) { precedes = true; break; }
                    }
                    if (precedes) break;
                }
            }

            if (precedes) {
                adj[i].push_back(j);
                inDegree[j]++;
            }
        }
    }

    // Kahn's algorithm for topological sorting
    std::vector<size_t> queue;
    for (size_t i = 0; i < n; ++i) {
        if (inDegree[i] == 0) {
            queue.push_back(i);
        }
    }

    std::vector<size_t> order;
    size_t head = 0;
    while (head < queue.size()) {
        size_t u = queue[head++];
        order.push_back(u);

        for (size_t v : adj[u]) {
            inDegree[v]--;
            if (inDegree[v] == 0) {
                queue.push_back(v);
            }
        }
    }

    // fallback for cycles
    if (order.size() < n) {
        LOG_WARN("RenderGraph: Dependency cycle or unresolved constraints detected! Falling back to registration order.");
        std::vector<bool> inOrder(n, false);
        for (size_t u : order) {
            inOrder[u] = true;
        }
        for (size_t i = 0; i < n; ++i) {
            if (!inOrder[i]) {
                order.push_back(i);
            }
        }
    }

    m_CompiledPasses.clear();
    m_CompiledPasses.reserve(n);
    for (size_t idx : order) {
        m_CompiledPasses.push_back(m_RegisteredPasses[idx]);
    }

    return true;
}

void RenderGraph::CalculateResourceLifetimes()
{
    for (auto& pair : m_Resources) {
        pair.second.firstPass = 0xFFFFFFFF;
        pair.second.lastPass = 0xFFFFFFFF;
    }

    for (uint32_t i = 0; i < static_cast<uint32_t>(m_CompiledPasses.size()); ++i) {
        const auto& pass = m_CompiledPasses[i];

        auto updateRes = [&](const std::string& name) {
            auto it = m_Resources.find(name);
            if (it != m_Resources.end()) {
                if (it->second.firstPass == 0xFFFFFFFF) {
                    it->second.firstPass = i;
                }
                it->second.lastPass = i;
            }
        };

        for (const auto& in : pass.inputs) {
            updateRes(in);
        }
        for (const auto& out : pass.outputs) {
            updateRes(out);
        }
    }
}

} // namespace eng::renderer
