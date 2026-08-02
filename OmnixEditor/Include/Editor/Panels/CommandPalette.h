#pragma once

#include "Runtime/RuntimeContext.h"
#include "Editor/EditorSelection.h"
#include "Editor/EditorDirtyState.h"
#include <string>
#include <vector>
#include <functional>

namespace eng::runtime {

    enum class CommandItemType {
        Entity,
        Asset,
        ConsoleCommand,
        Action
    };

    struct CommandItem {
        std::string label;
        std::string category;
        CommandItemType type;
        uint64_t id = 0; // Entity ID or AssetHandle value
        std::function<void()> action;
    };

    class CommandPalette {
    public:
        void Initialize(RuntimeContext* context);
        void ToggleOpen();
        void Open();
        void Close();
        bool IsOpen() const { return m_IsOpen; }

        void Render(EditorSelection& selection, EditorDirtyState& dirtyState);

    private:
        void RebuildSearchIndex();
        void ExecuteSelected(const CommandItem& item, EditorSelection& selection);

        RuntimeContext* m_Context = nullptr;
        bool m_IsOpen = false;
        bool m_FocusInput = false;
        char m_SearchBuffer[128] = "";
        int m_SelectedIndex = 0;
        std::vector<CommandItem> m_AllItems;
        std::vector<CommandItem> m_FilteredItems;
    };

} // namespace eng::runtime
