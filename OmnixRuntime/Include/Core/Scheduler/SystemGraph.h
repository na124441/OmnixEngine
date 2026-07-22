#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace eng::core {

    struct TaskNode {
        std::string name;
        std::function<void()> callback;
        std::vector<std::string> dependents; // Nodes that depend on this node
        std::vector<std::string> dependencies; // Nodes that this node depends on
    };

    class SystemGraph {
    public:
        SystemGraph() = default;
        ~SystemGraph() = default;

        void AddTask(const std::string& name, std::function<void()>&& callback);
        bool AddDependency(const std::string& taskName, const std::string& dependsOnName);
        
        bool Compile();
        const std::unordered_map<std::string, TaskNode>& GetNodes() const { return m_Nodes; }
        const std::vector<std::string>& GetTopologicalOrder() const { return m_TopologicalOrder; }

        void Clear();

    private:
        bool HasCycleDFS(const std::string& node, std::unordered_map<std::string, int>& visitedState);

        std::unordered_map<std::string, TaskNode> m_Nodes;
        std::vector<std::string> m_TopologicalOrder;
        bool m_IsCompiled = false;
    };

} // namespace eng::core
