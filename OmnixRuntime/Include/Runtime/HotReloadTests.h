#pragma once

namespace eng::runtime {
    bool RunTextureReloadTests() noexcept;
    bool RunShaderReloadTests() noexcept;
    bool RunMeshReloadTests() noexcept;
    bool RunHotReloadStressTests() noexcept;
}
