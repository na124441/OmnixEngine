#include "Runtime/Public/Editor/PlatformFileDialog.h"
#include <windows.h>
#include <commdlg.h>

namespace eng::editor {

std::string PlatformFileDialog::ShowOpenDialog(const char* filter) {
    OPENFILENAMEA ofn;
    char szFile[260] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    // OFN_NOCHANGEDIR is critical so that the working directory isn't mutated
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return szFile;
    }
    return "";
}

} // namespace eng::editor
