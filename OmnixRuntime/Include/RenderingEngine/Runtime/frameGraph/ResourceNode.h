#pragma once
#include <string>
#include "rhi/RHI.h"

namespace eng::runtime {

    /**
     * @class ResourceNode
     *
     * Represents a *virtual* resource inside the frame graph.
     *
     * A ResourceNode may correspond to:
     *   - A *swap‑chain image* (output to the screen)
     *   - A *transient* texture (e.g., G‑Buffer Albedo)
     *   - A *persistent* GPU buffer (e.g., camera uniform buffer)
     *
     * The node stores:
     *   - its *description* (format, dimensions, usage flags)
     *   - the *first* and *last* pass indices that access it (filled by the compiler)
     *   - an optional *alias* flag – if true the compiler may reuse the same memory
     *     as another transient resource that does not overlap in lifetime.
     */
    struct ResourceDesc {
        RHI::TextureDesc textureDesc;
        RHI::BufferDesc  bufferDesc;
        bool             isTexture{ true };   // if false, interpret as BufferDesc
        bool             isTransient{ false };
    };

    class ResourceNode {
    public:
        explicit ResourceNode(const std::string& name, const ResourceDesc& desc);
        ~ResourceNode() = default;

        const std::string& GetName() const noexcept { return m_Name; }
        const ResourceDesc& GetDesc() const noexcept { return m_Desc; }

        // Lifetime management – set by the GraphCompiler
        void SetFirstPass(uint32_t idx) noexcept { m_FirstPass = idx; }
        void SetLastPass(uint32_t idx) noexcept { m_LastPass = idx; }

        uint32_t FirstPass() const noexcept { return m_FirstPass; }
        uint32_t LastPass()  const noexcept { return m_LastPass; }

        bool IsTransient() const noexcept { return m_Desc.isTransient; }
        bool CanAlias()   const noexcept { return m_Desc.isTransient && m_Aliasable; }
        void SetAliasable(bool a) noexcept { m_Aliasable = a; }

    private:
        std::string   m_Name;
        ResourceDesc  m_Desc;
        uint32_t      m_FirstPass{ UINT32_MAX };
        uint32_t      m_LastPass{ 0 };
        bool          m_Aliasable{ false };
    };

} // namespace eng::runtime
