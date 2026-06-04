#include "Runtime/Public/PackageManager.h"
#include "Core/Logging/Logger.h"

namespace eng::runtime {

    PackageManager& GetPackageManager()
    {
        static PackageManager instance;
        return instance;
    }

    bool PackageManager::MountPackage(const std::filesystem::path& path)
    {
        std::filesystem::path normPath = std::filesystem::weakly_canonical(path);

        // Check if already mounted
        for (const auto& pkg : m_MountedPackages) {
            if (std::filesystem::weakly_canonical(pkg->GetPath()) == normPath) {
                LOG_WARN("[PackageManager] Package already mounted: %s", path.string().c_str());
                return true;
            }
        }

        auto pkg = std::make_unique<Package>();
        if (!pkg->Open(normPath)) {
            LOG_ERROR("[PackageManager] Failed to mount package: %s", path.string().c_str());
            return false;
        }

        m_MountedPackages.push_back(std::move(pkg));
        LOG_INFO("[PackageManager] Successfully mounted package: %s", path.string().c_str());
        return true;
    }

    bool PackageManager::UnmountPackage(const std::filesystem::path& path)
    {
        std::filesystem::path normPath = std::filesystem::weakly_canonical(path);

        for (auto it = m_MountedPackages.begin(); it != m_MountedPackages.end(); ++it) {
            if (std::filesystem::weakly_canonical((*it)->GetPath()) == normPath) {
                m_MountedPackages.erase(it);
                LOG_INFO("[PackageManager] Unmounted package: %s", path.string().c_str());
                return true;
            }
        }

        LOG_WARN("[PackageManager] Package not found to unmount: %s", path.string().c_str());
        return false;
    }

    const PackageAssetEntry* PackageManager::FindAsset(AssetHandle handle) const
    {
        // "Latest mounted package wins" lookup priority - search in reverse
        for (auto it = m_MountedPackages.rbegin(); it != m_MountedPackages.rend(); ++it) {
            if (const auto* entry = (*it)->FindAsset(handle)) {
                return entry;
            }
        }
        return nullptr;
    }

    std::vector<uint8_t> PackageManager::ReadAssetPayload(AssetHandle handle) const
    {
        for (auto it = m_MountedPackages.rbegin(); it != m_MountedPackages.rend(); ++it) {
            std::vector<uint8_t> payload = (*it)->ReadAssetPayload(handle);
            if (!payload.empty()) {
                return payload;
            }
        }
        return {};
    }

    bool PackageManager::Contains(AssetHandle handle) const
    {
        return FindAsset(handle) != nullptr;
    }

    std::vector<AssetHandle> PackageManager::GetDependencies(AssetHandle handle) const
    {
        for (auto it = m_MountedPackages.rbegin(); it != m_MountedPackages.rend(); ++it) {
            if ((*it)->FindAsset(handle)) {
                return (*it)->GetAssetDependencies(handle);
            }
        }
        return {};
    }

    void PackageManager::Clear()
    {
        m_MountedPackages.clear();
        LOG_INFO("[PackageManager] All mounted packages cleared.");
    }

} // namespace eng::runtime
