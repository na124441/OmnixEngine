#include "Core/Scheduler/SystemGraph.h"
#include <queue>

namespace eng::core {

    void SystemGraph::AddTask(const std::string& name, std::function<void()>&& callback) {
        if (m_Nodes.find(name) == m_Nodes.end()) {
            m_Nodes[name] = TaskNode{ name, std::move(callback), {}, {} };
            m_IsCompiled = false;
        }
    }

    bool SystemGraph::AddDependency(const std::string& taskName, const std::string& dependsOnName) {
        if (m_Nodes.find(taskName) == m_Nodes.end() || m_Nodes.find(dependsOnName) == m_Nodes.end()) {
            return false;
        }
        
        // Prevent duplicate dependencies
        auto& deps = m_Nodes[taskName].dependencies;
        for (const auto& dep : deps) {
            if (dep == dependsOnName) return true;
        }

        m_Nodes[taskName].dependencies.push_back(dependsOnName);
        m_Nodes[dependsOnName].dependents.push_back(taskName);
        m_IsCompiled = false;
        return true;
    }

    bool SystemGraph::Compile() {
        if (m_IsCompiled) return true;

        m_TopologicalOrder.clear();

        // Detect cycles first
        // 0 = unvisited, 1 = visiting, 2 = visited
        std::unordered_map<std::string, int> visitedState;
        for (const auto& [name, node] : m_Nodes) {
            visitedState[name] = 0;
        }

        for (const auto& [name, node] : m_Nodes) {
            if (visitedState[name] == 0) {
                if (HasCycleDFS(name, visitedState)) {
                    return false; // Cycle detected!
                }
            }
        }

        // Kahn's algorithm for topological sorting
        std::unordered_map<std::string, int> inDegrees;
        for (const auto& [name, node] : m_Nodes) {
            inDegrees[name] = static_cast<int>(node.dependencies.size());
        }

        std::queue<std::string> zeroInDegreeQueue;
        for (const auto& [name, node] : m_Nodes) {
            if (inDegrees[name] == 0) {
                zeroInDegreeQueue.push(name);
            }
        }

        while (!zeroInDegreeQueue.empty()) {
            std::string curr = zeroInDegreeQueue.front();
            zeroInDegreeQueue.pop();
            m_TopologicalOrder.push_back(curr);

            for (const auto& dep : m_Nodes[curr].dependents) {
                inDegrees[dep]--;
                if (inDegrees[dep] == 0) {
                    zeroInDegreeQueue.push(dep);
                }
            }
        }

        m_IsCompiled = (m_TopologicalOrder.size() == m_Nodes.size());
        return m_IsCompiled;
    }

    bool SystemGraph::HasCycleDFS(const std::string& node, std::unordered_map<std::string, int>& visitedState) {
        visitedState[node] = 1; // Visiting

        for (const auto& dep : m_Nodes[node].dependencies) {
            if (visitedState[dep] == 1) {
                return true; // Back-edge found (cycle)!
            }
            if (visitedState[dep] == 0) {
                if (HasCycleDFS(dep, visitedState)) {
                    return true;
                }
            }
        }

        visitedState[node] = 2; // Visited
        return false;
    }

    void SystemGraph::Clear() {
        m_Nodes.clear();
        m_TopologicalOrder.clear();
        m_IsCompiled = false;
    }

} // namespace eng::core
