cmake_minimum_required(VERSION 3.10)
project(OmnixEngine VERSION 1.0.0 LANGUAGES CXX)

# -------------------------------
# C++ Standard & MSVC Fixes
# -------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    add_compile_options(/Z7) 
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    
    # Enable high-performance optimization options for Release configuration
    add_compile_options("$<$<CONFIG:Release>:/O2>")
    add_compile_options("$<$<CONFIG:Release>:/Oi>")
    add_compile_options("$<$<CONFIG:Release>:/Ot>")
    add_compile_options("$<$<CONFIG:Release>:/Oy>")
    add_compile_options("$<$<CONFIG:Release>:/arch:AVX2>")
endif()

add_definitions(-DNOMINMAX -DENG_ENABLE_LOG -DENG_ENABLE_PROFILE -DGLM_FORCE_DEPTH_ZERO_TO_ONE)

option(OMNIX_WITH_PHYSX "Enable NVIDIA PhysX Physics System Support" ON)
if(OMNIX_WITH_PHYSX)
    add_definitions(-DOMNIX_WITH_PHYSX)
endif()

# -------------------------------
# External Dependencies
# -------------------------------
find_package(Vulkan REQUIRED)

# Find GLFW and PhysX dependencies
if(MSVC)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(unofficial-omniverse-physx-sdk CONFIG REQUIRED)
    set(GLFW_TARGET glfw)
else()
    set(GLFW_INCLUDE_DIR "C:/Users/nayan/vcpkg/packages/glfw3_x64-mingw-static/include")
    set(GLFW_LIBRARY "C:/Users/nayan/vcpkg/packages/glfw3_x64-mingw-static/lib/libglfw3.a")
    add_library(glfw_mingw INTERFACE)
    target_include_directories(glfw_mingw INTERFACE ${GLFW_INCLUDE_DIR})
    target_link_libraries(glfw_mingw INTERFACE ${GLFW_LIBRARY})
    set(GLFW_TARGET glfw_mingw)
endif()

# -------------------------------
# Global Include Directories
# -------------------------------
set(ENGINE_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Core
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/ECS
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/ECS/Public
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Scene
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Scene/Public
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Serializer
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Serializer/Serialization
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Components
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/RenderingEngine
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/RenderingEngine/Public
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty/spdlog/include
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty/VMA
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty/tinyglf
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty/stb
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Physics/Public

    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Rendering
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Runtime
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/src
    ${CMAKE_SOURCE_DIR}/OmnixRuntime/Include/Systems/Scheduler/Public

    ${CMAKE_SOURCE_DIR}/OmnixEditor/Include
    ${CMAKE_SOURCE_DIR}/OmnixEditor/Include/Editor
    ${CMAKE_SOURCE_DIR}/OmnixEditor/Include/Rendering/Editor
    ${CMAKE_SOURCE_DIR}/OmnixGame/Include
    ${CMAKE_SOURCE_DIR}/OmnixGame/Include/Gameplay
    ${CMAKE_SOURCE_DIR}/OmnixGame/Include/Components
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_SOURCE_DIR}/ThirdParty/nlohmann
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui/backends
    ${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo
    ${Vulkan_INCLUDE_DIRS}
)

# -------------------------------
# Libraries
# -------------------------------
add_library(imgui STATIC
    ThirdParty/imgui/imgui.cpp
    ThirdParty/imgui/imgui_draw.cpp
    ThirdParty/imgui/imgui_widgets.cpp
    ThirdParty/imgui/imgui_tables.cpp
    ThirdParty/imgui/backends/imgui_impl_win32.cpp
    ThirdParty/imgui/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${ENGINE_INCLUDE_DIRS})
target_link_libraries(imgui PUBLIC Vulkan::Vulkan dwmapi)

# -- Layer 1: OmnixRuntime --
add_library(OmnixRuntime STATIC
    OmnixRuntime/Source/Core/Diagnostics/Diagnostics.cpp
    OmnixRuntime/Source/Core/Diagnostics/StressTest.cpp
    OmnixRuntime/Source/Core/Logging/Logger.cpp
    OmnixRuntime/Source/Core/Memory/AllocationTracker.cpp
    OmnixRuntime/Source/Core/Memory/AllocatorValidation.cpp
    OmnixRuntime/Source/Core/Memory/FreeListAllocator.cpp
    OmnixRuntime/Source/Core/Memory/LinearAllocator.cpp
    OmnixRuntime/Source/Core/Memory/PoolAllocator.cpp
    OmnixRuntime/Source/Core/Memory/StackAllocator.cpp
    OmnixRuntime/Source/Core/Timer.cpp
    OmnixRuntime/Source/Core/Job/JobSystem.cpp
    OmnixRuntime/Source/Core/Job/Fiber.cpp
    OmnixRuntime/Source/Core/Scheduler/SystemScheduler.cpp
    OmnixRuntime/Source/Core/Scheduler/SystemGraph.cpp
    OmnixRuntime/Source/Core/IO/BinaryReader.cpp
    OmnixRuntime/Source/Core/IO/BinaryWriter.cpp
    OmnixRuntime/Source/Core/IO/FileWatcher.cpp
    OmnixRuntime/Source/Core/IO/FileSystem.cpp
    OmnixRuntime/Source/ECS/ComponentManager.cpp
    OmnixRuntime/Source/ECS/Coordinator.cpp
    OmnixRuntime/Source/ECS/EntityManager.cpp
    OmnixRuntime/Source/ECS/SystemManager.cpp
    OmnixRuntime/Source/Input/InputManager.cpp
    OmnixRuntime/Source/Physics/Private/PhysicsConversion.cpp
    OmnixRuntime/Source/Physics/Private/PhysicsDebugDraw.cpp
    OmnixRuntime/Source/Physics/Private/PhysicsValidation.cpp
    OmnixRuntime/Source/Physics/Private/PhysicsWorld.cpp
    OmnixRuntime/Source/Rendering/Core/FramebufferManager.cpp
    OmnixRuntime/Source/Rendering/Core/RenderTargetManager.cpp
    OmnixRuntime/Source/Rendering/Core/Renderer.cpp
    OmnixRuntime/Source/Rendering/Debug/DebugDraw.cpp
    OmnixRuntime/Source/Rendering/Debug/GPUProfiler.cpp
    OmnixRuntime/Source/Rendering/GPUScene/GPUScene.cpp
    OmnixRuntime/Source/Rendering/Geometry/Arena/GeometryAllocator.cpp
    OmnixRuntime/Source/Rendering/Geometry/Arena/GeometryArena.cpp
    OmnixRuntime/Source/Rendering/Geometry/Arena/GeometryArenaStats.cpp
    OmnixRuntime/Source/Rendering/Geometry/Arena/GeometryUploader.cpp
    OmnixRuntime/Source/Rendering/Geometry/Assets/RVGAsset.cpp
    OmnixRuntime/Source/Rendering/Geometry/Assets/RVGRegistry.cpp
    OmnixRuntime/Source/Rendering/Geometry/CapabilityTiers.cpp
    OmnixRuntime/Source/Rendering/Geometry/MeshRenderer.cpp
    OmnixRuntime/Source/Rendering/Geometry/Streaming/RVGPageStreamingManager.cpp
    OmnixRuntime/Source/Rendering/Geometry/VisibilitySystem.cpp
    OmnixRuntime/Source/Rendering/Graph/RenderGraph.cpp
    OmnixRuntime/Source/Rendering/Lighting/DeferredLightingPass.cpp
    OmnixRuntime/Source/Rendering/Lighting/EnvironmentSystem.cpp
    OmnixRuntime/Source/Rendering/Lighting/ShadowRenderer.cpp
    OmnixRuntime/Source/Rendering/Lighting/SkyLightRenderer.cpp
    OmnixRuntime/Source/Rendering/Materials/MaterialSystem.cpp
    OmnixRuntime/Source/Rendering/Materials/ShaderLibrary.cpp
    OmnixRuntime/Source/Rendering/PostProcess/ExposurePass.cpp
    OmnixRuntime/Source/Rendering/PostProcess/TonemapPass.cpp
    OmnixRuntime/Source/Rendering/Scene/RenderSceneExtractor.cpp
    OmnixRuntime/Source/Rendering/Visibility/Frustum.cpp
    OmnixRuntime/Source/Rendering/Visibility/FrustumCullPass.cpp
    OmnixRuntime/Source/Rendering/Visibility/HZBPass.cpp
    OmnixRuntime/Source/Rendering/Visibility/IndirectCommandBuildPass.cpp
    OmnixRuntime/Source/Rendering/Visibility/OcclusionCullPass.cpp
    OmnixRuntime/Source/Rendering/Visibility/RVGClusterCullPass.cpp
    OmnixRuntime/Source/RenderingEngine/Core/Engine/DebugLabels.cpp
    OmnixRuntime/Source/RenderingEngine/Core/Engine/EngineResources.cpp
    OmnixRuntime/Source/RenderingEngine/Platform/Time/Timer.cpp
    OmnixRuntime/Source/Core/Platform/Window.cpp
    OmnixRuntime/Source/Core/Platform/Monitor.cpp
    OmnixRuntime/Source/Core/Platform/Input.cpp
    OmnixRuntime/Source/Core/Platform/PlatformFileSystem.cpp
    OmnixRuntime/Source/Core/Platform/DynamicLibrary.cpp
    OmnixRuntime/Source/Core/Platform/Clipboard.cpp
    OmnixRuntime/Source/Core/Platform/Timer.cpp
    OmnixRuntime/Source/Core/Platform/Thread.cpp
    OmnixRuntime/Source/Core/Engine/EngineRuntime.cpp
    OmnixRuntime/Source/Core/Engine/Layer.cpp
    OmnixRuntime/Source/Core/Engine/LayerStack.cpp
    OmnixRuntime/Source/Core/Engine/ModuleManager.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/gltf/GltfModel.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/scene/Material.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/scene/Mesh.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/scene/ModelLoader.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/scene/Scene.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/scene/Shader.cpp
    OmnixRuntime/Source/RenderingEngine/Renderer/scene/Texture.cpp
    OmnixRuntime/Source/RenderingEngine/Runtime/Render_Scene/SceneBuilder.cpp
    OmnixRuntime/Source/RenderingEngine/Runtime/engine/EngineLoop.cpp
    OmnixRuntime/Source/RenderingEngine/Vulkan/VulkanDevice.cpp
    OmnixRuntime/Source/RenderingEngine/Vulkan/VulkanInstance.cpp
    OmnixRuntime/Source/RenderingEngine/Vulkan/VulkanMemory.cpp
    OmnixRuntime/Source/RenderingEngine/Vulkan/VulkanPipelineCache.cpp
    OmnixRuntime/Source/RenderingEngine/Vulkan/VulkanSwapChain.cpp
    OmnixRuntime/Source/Runtime/AllocationDiagnostics.cpp
    OmnixRuntime/Source/Runtime/AssetCacheTests.cpp
    OmnixRuntime/Source/Runtime/AssetLoadingStressTests.cpp
    OmnixRuntime/Source/Runtime/AssetManager.cpp
    OmnixRuntime/Source/Runtime/AssetRegistry.cpp
    OmnixRuntime/Source/Runtime/AssetRegistryTests.cpp
    OmnixRuntime/Source/Runtime/Audio/AudioSystem.cpp
    OmnixRuntime/Source/Runtime/Audio/MiniaudioBackend.cpp
    OmnixRuntime/Source/Runtime/BinaryReader.cpp
    OmnixRuntime/Source/Runtime/BinaryWriter.cpp
    OmnixRuntime/Source/Runtime/CVarSystem.cpp
    OmnixRuntime/Source/Runtime/ConfigSystem.cpp
    OmnixRuntime/Source/Runtime/FileWatcher.cpp
    OmnixRuntime/Source/Runtime/FormatTests.cpp
    OmnixRuntime/Source/Runtime/ContainerTests.cpp
    OmnixRuntime/Source/Runtime/ErrorTests.cpp
    OmnixRuntime/Source/Runtime/MathTests.cpp
    OmnixRuntime/Source/Runtime/JobTests.cpp
    OmnixRuntime/Source/Runtime/SchedulerTests.cpp
    OmnixRuntime/Source/Runtime/FileTests.cpp
    OmnixRuntime/Source/Runtime/WindowTests.cpp
    OmnixRuntime/Source/Runtime/MonitorTests.cpp
    OmnixRuntime/Source/Runtime/InputTests.cpp
    OmnixRuntime/Source/Runtime/PlatformFileSystemTests.cpp
    OmnixRuntime/Source/Runtime/DynamicLibraryTests.cpp
    OmnixRuntime/Source/Runtime/ClipboardTests.cpp
    OmnixRuntime/Source/Runtime/TimerTests.cpp
    OmnixRuntime/Source/Runtime/ThreadTests.cpp
    OmnixRuntime/Source/Runtime/LayerTests.cpp
    OmnixRuntime/Source/Runtime/GLTFImporter.cpp
    OmnixRuntime/Source/Runtime/GPUSceneTests.cpp
    OmnixRuntime/Source/Runtime/GeometryArenaTests.cpp
    OmnixRuntime/Source/Runtime/GeometryHandleTests.cpp
    OmnixRuntime/Source/Runtime/GoldenImageTests.cpp
    OmnixRuntime/Source/Runtime/HotReloadStressTests.cpp
    OmnixRuntime/Source/Runtime/HotReloadSystem.cpp
    OmnixRuntime/Source/Runtime/MeshImportTests.cpp
    OmnixRuntime/Source/Runtime/MeshImporter.cpp
    OmnixRuntime/Source/Runtime/MeshReloadTests.cpp
    OmnixRuntime/Source/Runtime/OBJImporter.cpp
    OmnixRuntime/Source/Runtime/OwnershipValidation.cpp
    OmnixRuntime/Source/Runtime/Package.cpp
    OmnixRuntime/Source/Runtime/PackageBuilder.cpp
    OmnixRuntime/Source/Runtime/PackageManager.cpp
    OmnixRuntime/Source/Runtime/PackageTests.cpp
    OmnixRuntime/Source/Runtime/PluginManager.cpp
    OmnixRuntime/Source/Runtime/RuntimeConsole.cpp
    OmnixRuntime/Source/Runtime/RuntimeDiagnostics.cpp
    OmnixRuntime/Source/Runtime/RuntimeLoaders.cpp
    OmnixRuntime/Source/Runtime/ShaderReloadTests.cpp
    OmnixRuntime/Source/Runtime/TextureImportTests.cpp
    OmnixRuntime/Source/Runtime/TextureImporter.cpp
    OmnixRuntime/Source/Runtime/TextureReloadTests.cpp
    OmnixRuntime/Source/Core/Time/TimeManager.cpp
    OmnixRuntime/Source/Core/UUID/UUID.cpp
    OmnixRuntime/Source/Runtime/World/WorldFileReader.cpp
    OmnixRuntime/Source/Runtime/World/WorldFileWriter.cpp
    OmnixRuntime/Source/Runtime/World/WorldManager.cpp
    OmnixRuntime/Source/Runtime/World/WorldZone.cpp
    OmnixRuntime/Source/Runtime/World/WorldZoneReader.cpp
    OmnixRuntime/Source/Runtime/World/WorldZoneWriter.cpp
    OmnixRuntime/Source/Scene/Camera.cpp
    OmnixRuntime/Source/Scene/ComponentFactory.cpp
    OmnixRuntime/Source/Scene/Prefab.cpp
    OmnixRuntime/Source/Scene/PrefabRegistry.cpp
    OmnixRuntime/Source/Scene/Scene.cpp
    OmnixRuntime/Source/Scene/SceneLoader.cpp
    OmnixRuntime/Source/Scene/SceneManager.cpp
    OmnixRuntime/Source/Scene/SceneObject.cpp
    OmnixRuntime/Source/Scene/SceneSerializer.cpp
    OmnixRuntime/Source/Scene/SceneValidator.cpp
    OmnixRuntime/Source/Scene/Transform.cpp
    OmnixRuntime/Source/Serializer/ECS/ECS.cpp
    OmnixRuntime/Source/Serializer/ECS/SchemaRegistry.cpp
    OmnixRuntime/Source/Serializer/ECS/SerializationBridge.cpp
    OmnixRuntime/Source/Serializer/ECS/Snapshot/ComponentSnapshot.cpp
    OmnixRuntime/Source/Serializer/ECS/Snapshot/ECSSnapshot.cpp
    OmnixRuntime/Source/Serializer/ECS/Snapshot/FieldSnapshot.cpp
    OmnixRuntime/Source/Core/Serialization/Delta/DeltaDeserializer.cpp
    OmnixRuntime/Source/Core/Serialization/Delta/DeltaSerializer.cpp
    OmnixRuntime/Source/Core/Serialization/Delta/DeltaSnapshot.cpp
    OmnixRuntime/Source/Core/Serialization/Delta/DeltaTracker.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/Binary/BinaryDeserializer.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/Binary/BinaryReader.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/Binary/BinarySerializer.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/Binary/BinaryWriter.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/NormalDeserializer.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/NormalSerializer.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/Text/TextDeserializer.cpp
    OmnixRuntime/Source/Core/Serialization/Normal/Text/TextSerializer.cpp
    RenderingEngine/ThirdParty/tinyglf/tiny_gltf_impl.cpp
    ThirdParty/ImGuizmo/ImGuizmo.cpp
)
target_include_directories(OmnixRuntime PUBLIC ${ENGINE_INCLUDE_DIRS})
target_link_libraries(OmnixRuntime PUBLIC Vulkan::Vulkan imgui "${GLFW_TARGET}")
if(MSVC)
    target_link_libraries(OmnixRuntime PUBLIC unofficial::omniverse-physx-sdk::sdk)
endif()

# -- Layer 2A: OmnixEditor --
add_library(OmnixEditor STATIC
    OmnixEditor/Source/Editor/AssetImportService.cpp
    OmnixEditor/Source/Editor/Commands/EditorEntityCommands.cpp
    OmnixEditor/Source/Editor/EditorDirtyState.cpp
    OmnixEditor/Source/Editor/EditorFileService.cpp
    OmnixEditor/Source/Editor/EditorLayer.cpp
    OmnixEditor/Source/Editor/EditorLayout.cpp
    OmnixEditor/Source/Editor/EditorNotificationService.cpp
    OmnixEditor/Source/Editor/EditorSceneService.cpp
    OmnixEditor/Source/Editor/EditorSelection.cpp
    OmnixEditor/Source/Editor/EditorTheme.cpp
    OmnixEditor/Source/Editor/Panels/AssetBrowserPanel.cpp
    OmnixEditor/Source/Editor/Panels/CommandPalette.cpp
    OmnixEditor/Source/Editor/Panels/ConsolePanel.cpp
    OmnixEditor/Source/Editor/Panels/ImportLogPanel.cpp
    OmnixEditor/Source/Editor/Panels/InspectorPanel.cpp
    OmnixEditor/Source/Editor/Panels/ProfilerPanel.cpp
    OmnixEditor/Source/Editor/Panels/SceneHierarchyPanel.cpp
    OmnixEditor/Source/Editor/Panels/ViewportPanel.cpp
    OmnixEditor/Source/Editor/PlatformFileDialog.cpp
    OmnixEditor/Source/Editor/Utils/EditorMath.cpp
    OmnixEditor/Source/Editor/Utils/ImGuiHelpers.cpp
    OmnixEditor/Source/Editor/Widgets/ComponentWidgets.cpp
    OmnixEditor/Source/Editor/Widgets/TransformWidget.cpp
    OmnixEditor/Source/Rendering/Editor/DebugViewRenderer.cpp
    OmnixEditor/Source/Rendering/Editor/EditorOverlayPass.cpp
    OmnixEditor/Source/Rendering/Editor/EditorViewportRenderer.cpp
    OmnixEditor/Source/Rendering/Editor/GridRenderer.cpp
    OmnixEditor/Source/Rendering/Editor/SelectionOutlinePass.cpp
    ThirdParty/ImGuizmo/ImGuizmo.cpp
)
target_include_directories(OmnixEditor PUBLIC ${ENGINE_INCLUDE_DIRS})
target_link_libraries(OmnixEditor PUBLIC OmnixRuntime imgui)

# -- Layer 2B: OmnixGame --
add_library(OmnixGame STATIC
    OmnixGame/Source/Components/Behavior/MovementCapability.cpp
    OmnixGame/Source/Gameplay/CheckpointSystem.cpp
    OmnixGame/Source/Gameplay/GameMode.cpp
    OmnixGame/Source/Gameplay/GameplayEventBus.cpp
    OmnixGame/Source/Gameplay/Objectives/ObjectiveSystem.cpp
    OmnixGame/Source/Gameplay/Save/GameplaySaveSystem.cpp
    OmnixGame/Source/Gameplay/StateObjects/ObjectActivationSystem.cpp
    OmnixGame/Source/Gameplay/Systems/InteractionSystem.cpp
    OmnixGame/Source/Gameplay/UI/GameplayHUD.cpp
    OmnixGame/Source/Gameplay/Validation/GameplayValidator.cpp
    OmnixGame/Source/Gameplay/VerticalSliceGameMode.cpp
)
target_include_directories(OmnixGame PUBLIC ${ENGINE_INCLUDE_DIRS})
target_link_libraries(OmnixGame PUBLIC OmnixRuntime)

# -------------------------------
# Shader Compilation Target
# -------------------------------
find_program(GLSLC glslc PATHS ENV VULKAN_SDK)
if(GLSLC)
    add_custom_target(CompileShaders ALL
        COMMAND powershell.exe -Command "if (-Not (Test-Path '${CMAKE_SOURCE_DIR}/shaders')) { New-Item -ItemType Directory -Path '${CMAKE_SOURCE_DIR}/shaders' }"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/pbr_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/pbr_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/pbr_frag.glsl -o ${CMAKE_SOURCE_DIR}/shaders/pbr_frag.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/depth_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/depth_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/shadow_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/shadow_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/gbuffer_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/gbuffer_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/gbuffer_frag.glsl -o ${CMAKE_SOURCE_DIR}/shaders/gbuffer_frag.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/fullscreen_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/fullscreen_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/deferred_lighting.glsl -o ${CMAKE_SOURCE_DIR}/shaders/deferred_lighting.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/postprocess_frag.glsl -o ${CMAKE_SOURCE_DIR}/shaders/postprocess_frag.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/transparent_frag.glsl -o ${CMAKE_SOURCE_DIR}/shaders/transparent_frag.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/grid_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/grid_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/grid_frag.glsl -o ${CMAKE_SOURCE_DIR}/shaders/grid_frag.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=comp ${CMAKE_SOURCE_DIR}/shaders/light_culling.comp -o ${CMAKE_SOURCE_DIR}/shaders/light_culling.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=comp ${CMAKE_SOURCE_DIR}/shaders/frustum_cull.comp -o ${CMAKE_SOURCE_DIR}/shaders/frustum_cull.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=vert ${CMAKE_SOURCE_DIR}/shaders/visibility_vert.glsl -o ${CMAKE_SOURCE_DIR}/shaders/visibility_vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/visibility_frag.glsl -o ${CMAKE_SOURCE_DIR}/shaders/visibility_frag.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/visibility_resolve.frag -o ${CMAKE_SOURCE_DIR}/shaders/visibility_resolve.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=mesh --target-env=vulkan1.3 ${CMAKE_SOURCE_DIR}/shaders/visibility.mesh -o ${CMAKE_SOURCE_DIR}/shaders/visibility_mesh.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=task --target-env=vulkan1.3 ${CMAKE_SOURCE_DIR}/shaders/visibility.task -o ${CMAKE_SOURCE_DIR}/shaders/visibility_task.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=comp ${CMAKE_SOURCE_DIR}/shaders/software_rasterizer.comp -o ${CMAKE_SOURCE_DIR}/shaders/software_rasterizer.spv"
        COMMAND powershell.exe -Command "${GLSLC} -fshader-stage=frag ${CMAKE_SOURCE_DIR}/shaders/dof.frag -o ${CMAKE_SOURCE_DIR}/shaders/dof.spv"
        COMMENT "Compiling Vulkan shaders..."
    )
    add_dependencies(OmnixRuntime CompileShaders)
endif()

# Precompiled Header Configuration
if(COMMAND target_precompiled_headers)
    target_precompiled_headers(OmnixRuntime PRIVATE OmnixRuntime/Include/RenderingEngine/Core/pch.h)
endif()

# -------------------------------
# Applications
# -------------------------------
add_executable(Application
    OmnixApp/Source/main.cpp
    OmnixApp/Source/Core/Application.cpp
)
target_link_libraries(Application PRIVATE OmnixGame OmnixEditor OmnixRuntime "${GLFW_TARGET}")
target_include_directories(Application PRIVATE ${ENGINE_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/OmnixApp/Include)

# -------------------------------
# Tools & Schedulers
# -------------------------------
add_executable(OmnixRVGCooker
    Tools/RVGCooker/RVGCookerMain.cpp
    Tools/RVGCooker/MeshCanonicalizer.cpp
    Tools/RVGCooker/MeshAdjacency.cpp
    Tools/RVGCooker/ClusterBuilder.cpp
    Tools/RVGCooker/ClusterGroupBuilder.cpp
    Tools/RVGCooker/ClusterSimplifier.cpp
    Tools/RVGCooker/HierarchyBuilder.cpp
    Tools/RVGCooker/RVGPagePacker.cpp
    Tools/RVGCooker/RVGWriter.cpp
)
target_link_libraries(OmnixRVGCooker PRIVATE OmnixRuntime)
target_include_directories(OmnixRVGCooker PRIVATE ${ENGINE_INCLUDE_DIRS})

add_executable(sampler Tools/ECSSampler/sampler.cpp)
target_link_libraries(sampler PRIVATE OmnixRuntime)
target_include_directories(sampler PRIVATE ${ENGINE_INCLUDE_DIRS})

add_executable(transform_tests OmnixRuntime/Source/Scene/tests/TransformTests.cpp)
target_link_libraries(transform_tests PRIVATE OmnixRuntime)
target_include_directories(transform_tests PRIVATE ${ENGINE_INCLUDE_DIRS})
