#include "Runtime/Public/Editor/AssetImportService.h"
#include "Core/Logging/Logger.h"
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace eng::runtime {

std::string AssetImportService::ImportModel(const std::string& sourcePath, AssetRegistry* registry) {
    if (sourcePath.empty() || !registry) {
        CORE_LOG_ERROR("[AssetImportService] Invalid source path or registry.");
        return "";
    }

    try {
        std::filesystem::path srcPath(sourcePath);
        if (!std::filesystem::exists(srcPath)) {
            CORE_LOG_ERROR("[AssetImportService] Source file does not exist: %s", sourcePath.c_str());
            return "";
        }

        std::filesystem::path destDir("Assets/Models");
        std::filesystem::create_directories(destDir);

        std::filesystem::path destPath = destDir / srcPath.filename();

        // Prevent overwrite without confirmation on Windows
        if (std::filesystem::exists(destPath)) {
#ifdef _WIN32
            int msgboxID = MessageBoxA(
                NULL,
                ("The file " + destPath.filename().string() + " already exists in Assets/Models. Do you want to overwrite it?").c_str(),
                "Confirm Overwrite",
                MB_ICONQUESTION | MB_YESNO
            );
            if (msgboxID == IDNO) {
                CORE_LOG_INFO("[AssetImportService] Import cancelled by user (overwrite declined).");
                return "";
            }
#endif
        }

        std::filesystem::copy_file(srcPath, destPath, std::filesystem::copy_options::overwrite_existing);

        // Copy optional associated .mtl file
        std::filesystem::path mtlSrcPath = srcPath;
        mtlSrcPath.replace_extension(".mtl");
        if (std::filesystem::exists(mtlSrcPath)) {
            std::filesystem::path mtlDestPath = destPath;
            mtlDestPath.replace_extension(".mtl");
            std::filesystem::copy_file(mtlSrcPath, mtlDestPath, std::filesystem::copy_options::overwrite_existing);
        }

        std::string relPath = destPath.string();
        std::replace(relPath.begin(), relPath.end(), '\\', '/');

        // Register asset
        registry->RegisterAsset(relPath, AssetType::Mesh);
        registry->SaveRegistry("AssetRegistry.json");

        CORE_LOG_INFO("[AssetImportService] Successfully imported model to %s", relPath.c_str());
        return relPath;
    } catch (const std::exception& e) {
        CORE_LOG_ERROR("[AssetImportService] Failed to import model: %s", e.what());
        return "";
    }
}

} // namespace eng::runtime
