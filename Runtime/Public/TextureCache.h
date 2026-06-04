#pragma once
#include "Runtime/Public/AssetHandle.h"
#include "Runtime/Public/TextureMetadata.h"
#include <unordered_map>
#include <string>
#include <filesystem>

namespace eng::runtime {

    class TextureCache
    {
    public:
        TextureCache() = default;
        ~TextureCache() = default;

        /**
         * @brief Checks if a cached texture exists and is newer than or equal to the source file.
         */
        bool IsCachedAndUpToDate(const std::string& sourcePath, const std::string& cachePath) const {
            if (!std::filesystem::exists(cachePath) || !std::filesystem::exists(cachePath + ".meta")) {
                return false;
            }

            try {
                auto sourceTime = std::filesystem::last_write_time(sourcePath);
                auto cacheTime = std::filesystem::last_write_time(cachePath);
                if (sourceTime > cacheTime) {
                    return false;
                }
            } catch (...) {
                return false;
            }

            return true;
        }

        /**
         * @brief Retrieves cached metadata from memory.
         */
        bool GetCachedMetadata(AssetHandle handle, TextureMetadata& outMeta) const {
            auto it = m_Cache.find(handle);
            if (it != m_Cache.end()) {
                outMeta = it->second;
                return true;
            }
            return false;
        }

        /**
         * @brief Stores metadata in memory.
         */
        void CacheMetadata(const TextureMetadata& meta) {
            m_Cache[meta.handle] = meta;
        }

        /**
         * @brief Clears memory cache.
         */
        void Clear() noexcept {
            m_Cache.clear();
        }

    private:
        std::unordered_map<AssetHandle, TextureMetadata> m_Cache;
    };

} // namespace eng::runtime
