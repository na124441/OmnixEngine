#pragma once
#include "Core/types/Handle.h"

namespace eng::rhi {

    struct BufferTag {};
    struct TextureTag {};
    struct ShaderTag {};
    struct PipelineTag {};
    struct DescriptorSetTag {};
    struct FenceTag {};
    struct SemaphoreTag {};

    using BufferHandle        = eng::core::Handle<BufferTag>;
    using TextureHandle       = eng::core::Handle<TextureTag>;
    using ShaderHandle        = eng::core::Handle<ShaderTag>;
    using PipelineHandle      = eng::core::Handle<PipelineTag>;
    using DescriptorSetHandle = eng::core::Handle<DescriptorSetTag>;
    using FenceHandle         = eng::core::Handle<FenceTag>;
    using SemaphoreHandle     = eng::core::Handle<SemaphoreTag>;

    class Device; // Forward declaration
}
