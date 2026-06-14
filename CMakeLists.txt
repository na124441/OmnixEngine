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
endif()

add_definitions(-DNOMINMAX -DENG_ENABLE_LOG -DENG_ENABLE_PROFILE)

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
    ${CMAKE_SOURCE_DIR}/Core
    ${CMAKE_SOURCE_DIR}/ECS
    ${CMAKE_SOURCE_DIR}/ECS/Public
    ${CMAKE_SOURCE_DIR}/Scene
    ${CMAKE_SOURCE_DIR}/Scene/Public
    ${CMAKE_SOURCE_DIR}/Serializer
    ${CMAKE_SOURCE_DIR}/Serializer/Serialization
    ${CMAKE_SOURCE_DIR}/Components
    ${CMAKE_SOURCE_DIR}/Data
    ${CMAKE_SOURCE_DIR}/RenderingEngine
    ${CMAKE_SOURCE_DIR}/RenderingEngine/Public
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty/tinyglf
    ${CMAKE_SOURCE_DIR}/RenderingEngine/ThirdParty/stb
    ${CMAKE_SOURCE_DIR}/Systems/Scheduler/Public
    ${CMAKE_SOURCE_DIR}/ThirdParty
    ${CMAKE_SOURCE_DIR}/ThirdParty/nlohmann
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui
    ${CMAKE_SOURCE_DIR}/ThirdParty/imgui/backends
    ${CMAKE_SOURCE_DIR}/ThirdParty/ImGuizmo
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/Physics/Public
    ${CMAKE_SOURCE_DIR}/Rendering
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

add_library(EngineCore
    Core/Logging/Logger.cpp
    Core/Diagnostics/Diagnostics.cpp
    Core/Timer.cpp
    Components/Behavior/MovementCapability.cpp
    Core/Memory/AllocationTracker.cpp
    Core/Memory/LinearAllocator.cpp
    Core/Memory/PoolAllocator.cpp
    Core/Memory/StackAllocator.cpp
    Core/Memory/AllocatorValidation.cpp
)
target_include_directories(EngineCore PUBLIC ${ENGINE_INCLUDE_DIRS})

add_library(Serialization
    Serializer/Serialization/Normal/NormalSerializer.cpp
    Serializer/Serialization/Normal/NormalDeserializer.cpp
    Serializer/Serialization/Normal/Binary/BinarySerializer.cpp
    Serializer/Serialization/Normal/Binary/BinaryDeserializer.cpp
    Serializer/Serialization/Normal/Binary/BinaryWriter.cpp
    Serializer/Serialization/Normal/Binary/BinaryReader.cpp
    Serializer/Serialization/Normal/Text/TextSerializer.cpp
    Serializer/Serialization/Normal/Text/TextDeserializer.cpp
    Serializer/Serialization/Delta/DeltaSerializer.cpp
    Serializer/Serialization/Delta/DeltaDeserializer.cpp
    Serializer/Serialization/Delta/DeltaTracker.cpp
    Serializer/Serialization/Delta/DeltaSnapshot.cpp
    Serializer/ECS/ECS.cpp
    Serializer/ECS/SchemaRegistry.cpp
    Serializer/ECS/SerializationBridge.cpp
    Serializer/ECS/Snapshot/ComponentSnapshot.cpp
    Serializer/ECS/Snapshot/FieldSnapshot.cpp
    Serializer/ECS/Snapshot/ECSSnapshot.cpp
)
target_link_libraries(Serialization PUBLIC EngineCore)
target_include_directories(Serialization PUBLIC ${ENGINE_INCLUDE_DIRS})

add_library(ECS
    ECS/EntityManager.cpp
    ECS/ComponentManager.cpp
    ECS/SystemManager.cpp
    ECS/Coordinator.cpp
)
target_link_libraries(ECS PUBLIC EngineCore Serialization)
target_include_directories(ECS PUBLIC ${ENGINE_INCLUDE_DIRS})

add_library(Input
    Input/InputManager.cpp
)
target_link_libraries(Input PUBLIC EngineCore)
target_include_directories(Input PUBLIC ${ENGINE_INCLUDE_DIRS})

add_library(Scene
    Scene/Scene.cpp
    Scene/SceneManager.cpp
    Scene/SceneObject.cpp
    Scene/Transform.cpp
    Scene/Camera.cpp
    Scene/Prefab.cpp
    Scene/PrefabRegistry.cpp
    Scene/ComponentFactory.cpp
    Scene/SceneLoader.cpp
    Scene/SceneSerializer.cpp
    Scene/SceneValidator.cpp
)
target_link_libraries(Scene PUBLIC EngineCore ECS)
target_include_directories(Scene PUBLIC ${ENGINE_INCLUDE_DIRS})

add_library(Physics STATIC
    Physics/Private/PhysicsWorld.cpp
    Physics/Private/PhysicsValidation.cpp
    Physics/Private/PhysicsDebugDraw.cpp
    Physics/Private/PhysicsConversion.cpp
)
target_include_directories(Physics PUBLIC ${ENGINE_INCLUDE_DIRS})
target_link_libraries(Physics PUBLIC EngineCore ECS)
if(MSVC)
    target_link_libraries(Physics PUBLIC unofficial::omniverse-physx-sdk::sdk)
endif()

add_library(RenderingEngine
    RenderingEngine/Runtime/engine/EngineLoop.cpp
    RenderingEngine/Platform/window/Window.cpp
    RenderingEngine/Vulkan/VulkanInstance.cpp
    RenderingEngine/Vulkan/VulkanDevice.cpp
    RenderingEngine/Vulkan/VulkanSwapChain.cpp
    RenderingEngine/Vulkan/VulkanMemory.cpp
    RenderingEngine/Platform/time/Timer.cpp
    RenderingEngine/runtime/render_scene/SceneBuilder.cpp
    RenderingEngine/Renderer/PyramidRenderer.cpp
    Rendering/Core/Renderer.cpp
    Rendering/Core/RenderTargetManager.cpp
    Rendering/Core/FramebufferManager.cpp
    Rendering/Graph/RenderGraph.cpp
    Rendering/Scene/RenderSceneExtractor.cpp
    Rendering/Scene/GPUScene.cpp
    Rendering/Geometry/MeshRenderer.cpp
    Rendering/Geometry/VisibilitySystem.cpp
    Rendering/Materials/MaterialSystem.cpp
    Rendering/Materials/ShaderLibrary.cpp
    Rendering/Lighting/DeferredLightingPass.cpp
    Rendering/Lighting/ShadowRenderer.cpp
    Rendering/Lighting/SkyLightRenderer.cpp
    Rendering/PostProcess/TonemapPass.cpp
    Rendering/PostProcess/ExposurePass.cpp
    Rendering/Editor/EditorViewportRenderer.cpp
    Rendering/Editor/GridRenderer.cpp
    Rendering/Editor/SelectionOutlinePass.cpp
    Rendering/Editor/DebugViewRenderer.cpp
    Rendering/Debug/GPUProfiler.cpp
    Rendering/Debug/DebugDraw.cpp
    RenderingEngine/Renderer/scene/Mesh.cpp
    RenderingEngine/Renderer/scene/ModelLoader.cpp
    RenderingEngine/Renderer/scene/Scene.cpp
    RenderingEngine/Renderer/scene/Material.cpp
    RenderingEngine/Renderer/scene/Texture.cpp
    RenderingEngine/Renderer/scene/Shader.cpp
    RenderingEngine/Renderer/gltf/GltfModel.cpp
    RenderingEngine/ThirdParty/tinyglf/tiny_gltf_impl.cpp
    RenderingEngine/Core/Engine/DebugLabels.cpp
    RenderingEngine/Core/Engine/EngineResources.cpp
)
target_include_directories(RenderingEngine PUBLIC ${ENGINE_INCLUDE_DIRS})
target_link_libraries(RenderingEngine PUBLIC EngineCore Vulkan::Vulkan "${GLFW_TARGET}" user32 gdi32 shell32 imgui)
if(COMMAND target_precompiled_headers)
    target_precompiled_headers(RenderingEngine PRIVATE RenderingEngine/Core/pch.h)
endif()

find_program(GLSLC glslc PATHS ENV VULKAN_SDK)
if(GLSLC)
    add_custom_target(CompileShaders ALL
        COMMAND powershell.exe -Command "${GLSLC} ${CMAKE_SOURCE_DIR}/RenderingEngine/Shaders/shader.vert -o ${CMAKE_SOURCE_DIR}/vert.spv"
        COMMAND powershell.exe -Command "${GLSLC} ${CMAKE_SOURCE_DIR}/RenderingEngine/Shaders/shader.frag -o ${CMAKE_SOURCE_DIR}/frag.spv"
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
        COMMENT "Compiling Vulkan shaders..."
    )
    add_dependencies(RenderingEngine CompileShaders)
endif()

# -------------------------------
# EngineRuntime Core
# -------------------------------
add_library(EngineRuntime
    Runtime/Private/EngineRuntime.cpp
    Runtime/Private/Gameplay/GameMode.cpp
    Runtime/Private/Gameplay/Save/GameplaySaveSystem.cpp
    Runtime/Private/Gameplay/VerticalSliceGameMode.cpp
    Runtime/Private/Gameplay/GameplayEventBus.cpp
    Runtime/Private/Gameplay/Objectives/ObjectiveSystem.cpp
    Runtime/Private/Gameplay/StateObjects/ObjectActivationSystem.cpp
    Runtime/Private/Gameplay/CheckpointSystem.cpp
    Runtime/Private/Gameplay/UI/GameplayHUD.cpp
    Runtime/Private/Gameplay/Systems/InteractionSystem.cpp
    Runtime/Private/Gameplay/Validation/GameplayValidator.cpp
    Runtime/Private/Audio/MiniaudioBackend.cpp
    Runtime/Private/Audio/AudioSystem.cpp
    Runtime/Private/Editor/EditorLayer.cpp
    Runtime/Private/Editor/EditorTheme.cpp
    Runtime/Private/Editor/EditorLayout.cpp
    Runtime/Private/Editor/PlatformFileDialog.cpp
    Runtime/Private/Editor/AssetImportService.cpp
    ThirdParty/ImGuizmo/ImGuizmo.cpp
    Runtime/Private/Editor/EditorSelection.cpp
    Runtime/Private/Editor/EditorDirtyState.cpp
    Runtime/Private/Editor/Utils/EditorMath.cpp
    Runtime/Private/Editor/Utils/ImGuiHelpers.cpp
    Runtime/Private/Editor/Commands/EditorEntityCommands.cpp
    Runtime/Private/Editor/EditorFileService.cpp
    Runtime/Private/Editor/EditorNotificationService.cpp
    Runtime/Private/Editor/EditorSceneService.cpp
    Runtime/Private/Editor/Panels/SceneHierarchyPanel.cpp
    Runtime/Private/Editor/Panels/InspectorPanel.cpp
    Runtime/Private/Editor/Panels/ConsolePanel.cpp
    Runtime/Private/Editor/Panels/ImportLogPanel.cpp
    Runtime/Private/Editor/Panels/ViewportPanel.cpp
    Runtime/Private/Editor/Panels/AssetBrowserPanel.cpp
    Runtime/Private/Editor/Widgets/TransformWidget.cpp
    Runtime/Private/Editor/Widgets/ComponentWidgets.cpp
    Runtime/Private/RuntimeDiagnostics.cpp
    Runtime/Private/AllocationDiagnostics.cpp
    Runtime/Private/OwnershipValidation.cpp
    Core/Diagnostics/StressTest.cpp
    Runtime/Private/AssetRegistry.cpp
    Runtime/Private/AssetRegistryTests.cpp
    Runtime/Private/BinaryWriter.cpp
    Runtime/Private/BinaryReader.cpp
    Runtime/Private/World/WorldFileReader.cpp
    Runtime/Private/World/WorldFileWriter.cpp
    Runtime/Private/World/WorldZone.cpp
    Runtime/Private/World/WorldZoneReader.cpp
    Runtime/Private/World/WorldZoneWriter.cpp
    Runtime/Private/World/WorldManager.cpp
    Runtime/Private/FormatTests.cpp
    Runtime/Private/TextureImporter.cpp
    Runtime/Private/TextureImportTests.cpp
    Runtime/Private/OBJImporter.cpp
    Runtime/Private/GLTFImporter.cpp
    Runtime/Private/MeshImporter.cpp
    Runtime/Private/MeshImportTests.cpp
    Runtime/Private/AssetManager.cpp
    Runtime/Private/RuntimeLoaders.cpp
    Runtime/Private/AssetCacheTests.cpp
    Runtime/Private/AssetLoadingStressTests.cpp
    Runtime/Private/FileWatcher.cpp
    Runtime/Private/HotReloadSystem.cpp
    Runtime/Private/TextureReloadTests.cpp
    Runtime/Private/ShaderReloadTests.cpp
    Runtime/Private/MeshReloadTests.cpp
    Runtime/Private/HotReloadStressTests.cpp
    Runtime/Private/PackageBuilder.cpp
    Runtime/Private/Package.cpp
    Runtime/Private/PackageManager.cpp
    Runtime/Private/PackageTests.cpp
)
target_link_libraries(EngineRuntime PUBLIC EngineCore ECS Serialization Input Scene RenderingEngine imgui Physics)
target_include_directories(EngineRuntime PUBLIC ${ENGINE_INCLUDE_DIRS})

# -------------------------------
# Applications
# -------------------------------
add_executable(Application
    main.cpp
    Core/Application.cpp
)
target_link_libraries(Application PRIVATE EngineRuntime "${GLFW_TARGET}")
target_include_directories(Application PRIVATE ${ENGINE_INCLUDE_DIRS})

# add_executable(serialization_test Test/SerializationTest.cpp)
# target_link_libraries(serialization_test PRIVATE EngineCore ECS Serialization)
# target_include_directories(serialization_test PRIVATE ${ENGINE_INCLUDE_DIRS})

add_executable(sampler ECS/sampler.cpp)
target_link_libraries(sampler PRIVATE EngineCore ECS)
target_include_directories(sampler PRIVATE ${ENGINE_INCLUDE_DIRS})

add_executable(transform_tests Scene/tests/TransformTests.cpp)
target_link_libraries(transform_tests PRIVATE Scene EngineCore)
target_include_directories(transform_tests PRIVATE ${ENGINE_INCLUDE_DIRS})

# Phase 2: RenderQueue Unit Test
# add_executable(engine_tests RenderingEngine/Renderer/tests/RenderQueue_test.cpp)
# target_include_directories(engine_tests PRIVATE ${ENGINE_INCLUDE_DIRS})
# target_link_libraries(engine_tests PRIVATE EngineCore RenderingEngine)