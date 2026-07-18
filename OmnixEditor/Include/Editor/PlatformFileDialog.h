#pragma once
#include <string>

namespace eng::editor {

class PlatformFileDialog {
public:
    // Opens a file dialog and returns the absolute path of the chosen file.
    // Returns an empty string if cancelled.
    static std::string ShowOpenDialog(const char* filter);

    // Opens a save file dialog and returns the absolute path selected by the user.
    // Returns an empty string if cancelled.
    static std::string ShowSaveDialog(const char* filter, const char* defaultExtension);
};

} // namespace eng::editor
