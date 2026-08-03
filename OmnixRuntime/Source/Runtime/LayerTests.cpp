#include "Runtime/LayerTests.h"
#include "Core/Engine/LayerStack.h"
#include "Core/Logger.h"
#include "Core/Diagnostics/Assert.h"
#include <vector>
#include <string>

namespace eng::runtime {

    class TestLayer : public eng::core::Layer {
    public:
        TestLayer(const std::string& name, std::vector<std::string>& log)
            : eng::core::Layer(name), m_Log(log) {}

        void OnAttach() override {
            m_Attached = true;
            m_Log.push_back(m_DebugName + ":Attach");
        }

        void OnDetach() override {
            m_Attached = false;
            m_Log.push_back(m_DebugName + ":Detach");
        }

        void OnUpdate(float) override {
            m_Log.push_back(m_DebugName + ":Update");
        }

        void OnRender() override {
            m_Log.push_back(m_DebugName + ":Render");
        }

        void OnEvent(void*) override {
            m_Log.push_back(m_DebugName + ":Event");
        }

        bool m_Attached = false;

    private:
        std::vector<std::string>& m_Log;
    };

    bool RunLayerTests() {
        LOG_INFO("=== Running Kernel Layer & LayerStack Subsystem Tests ===");

        std::vector<std::string> log;

        eng::core::LayerStack stack;
        OMNIX_ASSERT(stack.IsEmpty(), "New LayerStack should be empty.");
        OMNIX_ASSERT(stack.GetSize() == 0, "Initial size should be 0.");

        LOG_INFO("[Test 1] Pushing regular layers (Layer 1, Layer 2)...");
        auto l1 = std::make_shared<TestLayer>("Layer1", log);
        auto l2 = std::make_shared<TestLayer>("Layer2", log);
        stack.PushLayer(l1);
        stack.PushLayer(l2);

        OMNIX_ASSERT(stack.GetSize() == 2, "Stack size should be 2 after pushing 2 layers.");
        OMNIX_ASSERT(stack.GetLayerInsertIndex() == 2, "LayerInsertIndex should be 2.");

        LOG_INFO("[Test 2] Pushing overlays (Overlay 1, Overlay 2)...");
        auto o1 = std::make_shared<TestLayer>("Overlay1", log);
        auto o2 = std::make_shared<TestLayer>("Overlay2", log);
        stack.PushOverlay(o1);
        stack.PushOverlay(o2);

        OMNIX_ASSERT(stack.GetSize() == 4, "Stack size should be 4 after pushing overlays.");
        OMNIX_ASSERT(stack.GetLayerInsertIndex() == 2, "LayerInsertIndex should remain 2 after pushing overlays.");

        LOG_INFO("[Test 3] Testing OnUpdate and OnRender execution order...");
        log.clear();
        stack.OnUpdate(0.016f);
        OMNIX_ASSERT(log.size() == 4, "Should log 4 updates.");
        OMNIX_ASSERT(log[0] == "Layer1:Update", "Layer1 should update first.");
        OMNIX_ASSERT(log[1] == "Layer2:Update", "Layer2 should update second.");
        OMNIX_ASSERT(log[2] == "Overlay1:Update", "Overlay1 should update third.");
        OMNIX_ASSERT(log[3] == "Overlay2:Update", "Overlay2 should update fourth.");

        LOG_INFO("[Test 4] Testing OnEvent reverse propagation order...");
        log.clear();
        stack.OnEvent(nullptr);
        OMNIX_ASSERT(log.size() == 4, "Should log 4 events.");
        OMNIX_ASSERT(log[0] == "Overlay2:Event", "Overlay2 (top overlay) should handle event first.");
        OMNIX_ASSERT(log[1] == "Overlay1:Event", "Overlay1 should handle event second.");
        OMNIX_ASSERT(log[2] == "Layer2:Event", "Layer2 should handle event third.");
        OMNIX_ASSERT(log[3] == "Layer1:Event", "Layer1 should handle event fourth.");

        LOG_INFO("[Test 5] Popping Layer 1 and verifying re-indexing...");
        log.clear();
        stack.PopLayer(l1);
        OMNIX_ASSERT(stack.GetSize() == 3, "Stack size should be 3.");
        OMNIX_ASSERT(stack.GetLayerInsertIndex() == 1, "LayerInsertIndex should be 1 after popping a layer.");

        LOG_INFO("[Test 6] Clearing stack...");
        log.clear();
        stack.Clear();
        OMNIX_ASSERT(stack.IsEmpty(), "Stack should be empty after Clear().");
        OMNIX_ASSERT(stack.GetLayerInsertIndex() == 0, "LayerInsertIndex should be reset to 0.");

        LOG_INFO("=== All Layer & LayerStack Tests Passed Successfully ===");
        return true;
    }

} // namespace eng::runtime
