# Omnix Engine - Complete File Structure Overview

## 📁 Project Root
- **main.cpp** - Main entry point and application initialization
- **build_and_test.bat** - Windows build and test automation script
- **build_and_test.sh** - Linux/macOS build and test automation script
- **CMakeSettings.json** - CMake configuration settings for IDEs
- **CppProperties.json** - C++ properties configuration
- **README.md** - Main project documentation (serialization focus)
- **ENGINE_COMPLETE_OVERVIEW.md** - Comprehensive engine architecture documentation
- **ARCHITECTURE_DIAGRAMS.md** - Visual system architecture diagrams
- **IMPLEMENTATION_COMPLETE.md** - Serialization implementation details
- **SERIALIZATION_TEST_README.md** - Test suite documentation
- **CMAKELISTS_CORRECTIONS.md** - CMake build system corrections
- **CMAKE_FIX_EXPLANATION.md** - CMake fixes explanation

## 🔧 Core Module
- **Core/Logger.h/cpp** - Debug logging system with severity levels and colored output
- **Core/Timer.h/cpp** - Frame timing, delta time calculation, and FPS tracking
- **Core/Application.cpp** - Main application entry point and game loop management
- **Core/World.h** - World container and management system

## 🎯 ECS (Entity Component System)
- **ECS/ECS.h/cpp** - Central ECS manager with entity/component lifecycle
- **ECS/EntityManager.h/cpp** - Entity ID generation and lifecycle management
- **ECS/ComponentManager.h/cpp** - Component pool management and storage
- **ECS/Coordinator.h/cpp** - System registration and update coordination
- **ECS/ECSConfig.h** - ECS configuration constants and limits
- **ECS/ECSComponents.h** - Standard component type definitions
- **ECS/Matrix4x4.h** - 4x4 matrix math operations
- **ECS/PhysicsSystem.h** - Physics simulation system implementation
- **ECS/RenderSystem.h** - Rendering system implementation
- **ECS/sampler.cpp** - ECS test and demonstration program

## 💾 Serialization Module
### Interfaces
- **Serializer/Serialization/ISerializer.h** - Abstract serializer interface contract
- **Serializer/Serialization/IDeserializer.h** - Abstract deserializer interface contract
- **Serializer/Serialization/SerializerContext.h** - Serialization configuration and behavior control
- **Serializer/Serialization/SerializationCommon.h** - Centralized serialization rules and validation

### Binary Implementation
- **Serializer/Serialization/Normal/NormalSerializer.h/cpp** - Binary format serializer implementation
- **Serializer/Serialization/Normal/NormalDeserializer.h/cpp** - Binary format deserializer implementation

### ECS Integration
- **Serializer/ECS/ECS.h/cpp** - ECS serialization bridge and integration
- **Serializer/ECS/SerializationBridge.h** - Facade for easy snapshot creation
- **Serializer/ECS/SchemaRegistry.h** - Component schema metadata registry
- **Serializer/ECS/ComponentSchema.h** - Component schema definition
- **Serializer/ECS/Component.h** - Component serialization interface
- **Serializer/ECS/Entity.h** - Entity serialization representation

### Snapshot System
- **Serializer/ECS/Snapshot/ECSSnapshot.h** - Complete world state snapshot container
- **Serializer/ECS/Snapshot/EntitySnapshot.h** - Individual entity state snapshot
- **Serializer/ECS/Snapshot/ComponentSnapshot.h** - Component data snapshot
- **Serializer/ECS/Snapshot/FieldSnapshot.h** - Individual field data snapshot
- **Serializer/ECS/Snapshot/SnapshotContext.h** - Snapshot creation context

### Binary Format Support
- **Serializer/Serialization/Normal/Binary/BinarySerializer.h/cpp** - Binary serializer base
- **Serializer/Serialization/Normal/Binary/BinaryDeserializer.h/cpp** - Binary deserializer base
- **Serializer/Serialization/Normal/Binary/BinaryWriter.h/cpp** - Binary data writer
- **Serializer/Serialization/Normal/Binary/BinaryReader.h/cpp** - Binary data reader

### Text Format Support
- **Serializer/Serialization/Normal/Text/TextSerializer.h/cpp** - Text format serializer
- **Serializer/Serialization/Normal/Text/TextDeserializer.h/cpp** - Text format deserializer
- **Serializer/Serialization/Normal/Text/TextWriter.h/cpp** - Text data writer
- **Serializer/Serialization/Normal/Text/TextReader.h/cpp** - Text data reader

### Delta Serialization
- **Serializer/Serialization/Delta/DeltaSerializer.h/cpp** - Delta (change-only) serializer
- **Serializer/Serialization/Delta/DeltaDeserializer.h/cpp** - Delta deserializer
- **Serializer/Serialization/Delta/DeltaTracker.h/cpp** - Change tracking system
- **Serializer/Serialization/Delta/DeltaSnapshot.h/cpp** - Delta snapshot representation
- **Serializer/Serialization/Delta/Binary/DeltaBinaryWriter.h** - Binary delta writer
- **Serializer/Serialization/Delta/Binary/DeltaBinaryReader.h** - Binary delta reader
- **Serializer/Serialization/Delta/Binary/DeltaBinaryCode.h** - Binary delta encoding

## 🎬 Scene Module
- **Scene/Scene.h/cpp** - Scene container and management
- **Scene/SceneManager.h/cpp** - Multiple scene management and transitions
- **Scene/SceneObject.h/cpp** - Base scene entity with transform and components
- **Scene/Transform.h/cpp** - Position, rotation, and scale transformations
- **Scene/Camera.h/cpp** - Perspective and orthographic camera implementation
- **Scene/Prefab.h/cpp** - Reusable object templates and instantiation
- **Scene/PrefabRegistry.h/cpp** - Prefab library management and lookup
- **Scene/ComponentFactory.h/cpp** - Component creation by type name
- **Scene/SceneLoader.h/cpp** - Scene file loading from various formats
- **Scene/SceneSerializer.h/cpp** - Scene serialization and deserialization
- **Scene/IDPool.h** - Object ID generation and management
- **Scene/Vector3.h** - 3D vector math operations
- **Scene/Quaternion.h** - Rotation quaternion implementation
- **Scene/Matrix4x4.h** - 4x4 transformation matrices

## ⚙️ Systems Module
### Core Systems
- **Systems/Core/System.h** - Base system interface
- **Systems/Core/SystemDescriptors.h** - System metadata and description
- **Systems/Core/SystemAccess.h** - System component access permissions
- **Systems/Core/SystemContext.h** - System execution context
- **Systems/Core/SystemState.h** - System runtime state management
- **Systems/Core/SystemTraits.h** - System type traits and metadata

### Scheduler
- **Systems/Scheduler/SystemScheduler.h** - System execution scheduling
- **Systems/Scheduler/SystemPhase.h** - Execution phase management
- **Systems/Scheduler/SystemDependencies.h** - System dependency resolution
- **Systems/Scheduler/SystemGraph.h** - System dependency graph
- **Systems/Scheduler/ExecutionPlan.h** - Execution plan generation

### Execution
- **Systems/Execution/FixedUpdateSystem.h** - Fixed timestep system execution
- **Systems/Execution/VariableUpdateSystem.h** - Variable timestep system execution
- **Systems/Execution/EventDrivenSystem.h** - Event-driven system execution
- **Systems/Execution/SystemExecutor.h** - System execution engine

### Registry
- **Systems/Registry/SystemRegistry.h** - System registration and lookup
- **Systems/Registry/SystemFactory.h** - System creation factory
- **Systems/Registry/SystemHandle.h** - System reference handles

### Simulation Systems
- **Systems/Types/SimulationSystems/RigidBody/SystemSnapshot.h** - Rigid body system state
- **Systems/Types/SimulationSystems/RigidBody/RigidBody_Integration.h** - Rigid body integration
- **Systems/Types/SimulationSystems/RigidBody/Constraint_Solver.h** - Constraint resolution
- **Systems/Types/SimulationSystems/RigidBody/Continous_Collision.h** - Continuous collision detection
- **Systems/Types/SimulationSystems/RigidBody/Sleeping_Activation.h** - Sleep state management
- **Systems/Types/SimulationSystems/RigidBody/Island_Generation.h** - Physics island generation
- **Systems/Types/SimulationSystems/RigidBody/Shock_Propagation.h** - Shock wave propagation

### Soft Body Systems
- **Systems/Types/SimulationSystems/SoftBody_Deformables/Mass-Spring_systems.h** - Mass-spring systems
- **Systems/Types/SimulationSystems/SoftBody_Deformables/FFM-based-Deformation.h** - Finite element deformation
- **Systems/Types/SimulationSystems/SoftBody_Deformables/Cloth_Simulation.h** - Cloth simulation
- **Systems/Types/SimulationSystems/SoftBody_Deformables/Rope-Cable-dynamics.h** - Rope and cable dynamics
- **Systems/Types/SimulationSystems/SoftBody_Deformables/Volume-preservartion.h** - Volume preservation
- **Systems/Types/SimulationSystems/SoftBody_Deformables/Tear-fracture-modelling.h** - Tearing and fracture

### Particle Systems
- **Systems/Types/SimulationSystems/Particle-Granual-Systems/Particle-Integration.h** - Particle integration
- **Systems/Types/SimulationSystems/Particle-Granual-Systems/Particle-particle-Interaction.h** - Particle interactions
- **Systems/Types/SimulationSystems/Particle-Granual-Systems/Particle-field-interaction.h** - Field interactions
- **Systems/Types/SimulationSystems/Particle-Granual-Systems/Granular-Flow.h** - Granular flow simulation
- **Systems/Types/SimulationSystems/Particle-Granual-Systems/SPH.h** - Smoothed Particle Hydrodynamics

### Fluid Dynamics
- **Systems/Types/SimulationSystems/Field-Gas-Dynamics/Grid-based-fluid-Solvers.h** - Grid-based fluid solvers
- **Systems/Types/SimulationSystems/Field-Gas-Dynamics/NaiverStokesIntegration.h** - Navier-Stokes integration
- **Systems/Types/SimulationSystems/Field-Gas-Dynamics/Voriticity-confinement.h** - Vorticity confinement
- **Systems/Types/SimulationSystems/Field-Gas-Dynamics/Smoke-fire-simulation.h** - Smoke and fire simulation
- **Systems/Types/SimulationSystems/极地-Gas-Dynamics/Ocean-wave-spectra.h** - Ocean wave simulation
- **Systems/Types/SimulationSystems/Field-Gas-Dynamics/Buoyancy-couplinh.h** - Buoyancy coupling

### Environmental Physics
- **Systems/Types/SimulationSystems/Environmental_Physics/WindFields.h** - Wind field simulation
- **Systems/Types/SimulationSystems/Environmental_Physics/WeatherSystems.h** - Weather system simulation
- **Systems/Types/SimulationSystems/Environmental_Physics/TemperatureDiffusion.h** - Temperature diffusion
- **Systems/Types/SimulationSystems/Environmental_Physics/PressureZones.h** - Pressure zone simulation
- **Systems/Types/SimulationSystems/Environmental_Physics/DestructionPropagation.h** - Destruction propagation

### Time Systems
- **Systems/Types/SimulationSystems/Time_Casuality/FixedTimeStepController.h** - Fixed timestep control
- **Systems/Types极地/SimulationSystems/Time_Casuality/TimeScalling.h** - Time scaling control
- **Systems/Types/SimulationSystems/Time_Casuality/Pause-slowmo.h** - Pause and slow motion
- **Systems/Types/SimulationSystems/Time_Casuality/Rewind-Rollback.h** - Rewind and rollback

### Spatial Systems
- **Systems/Types/SpatialWorldSystems/TransformHierarchy/Local-WorldTransformPropagation.h** - Transform hierarchy
- **Systems/Types/SpatialWorldSystems/TransformHierarchy/Parent-child-constraints.h** - Parent-child constraints
- **Systems/Types/S极地WorldSystems/TransformHierarchy/Attachment-systems.h** - Attachment systems
- **Systems/Types/SpatialWorldSystems/TransformHierarchy/Bone-space-systems.h** - Bone space systems

### Spatial Partitioning
- **Systems/Types/SpatialWorldSystems/SpatialPartitioning/BVH-KDTreeUpdates.h** - BVH and KD-tree updates
- **Systems/Types/SpatialWorldSystems/SpatialPartitioning/Octree-grid.h** - Octree and grid spatial partitioning
- **Systems/Types/SpatialWorldSystems/SpatialPartitioning/BroadphaseAcceleration.h** - Broadphase acceleration
- **Systems/Types/SpatialWorldSystems/SpatialPartitioning/VisibilityCuiling.h** - Visibility culling

### Navigation
- **Systems/Types/SpatialWorldSystems/Navigatio-PathFinding/Navmesh-generation.h** - Navmesh generation
- **Systems/Types/SpatialWorldSystems/Navigatio-PathFinding/Dynamic-obstacle-update.h** - Dynamic obstacle updates
- **Systems/Types/SpatialWorldSystems/Navigatio-PathFinding/DijkstaExecution.h** - Dijkstra pathfinding
- **Systems/Types/SpatialWorldSystems/Navigatio-PathFinding/CrowdSimulation.h** - Crowd simulation
- **Systems/Types/SpatialWorldSystems/Navigatio-PathFinding/FlowFields.h** - Flow field navigation

### World Streaming
- **Systems/Types/SpatialWorldSystems/WorldStreaming/ChunkLoadingUnloading.h** - Chunk loading/unloading
- **Systems/Types/SpatialWorldSystems/WorldStreaming/LODTransistions.h** - LOD transitions
- **Systems/Types/SpatialWorldSystems/WorldStreaming/MemoryBudgeting.h** - Memory budgeting
- **Systems/Types/SpatialWorldSystems/WorldStreaming/WorldOriginRebasing.h** - World origin rebasing

### Gameplay Logic
- **Systems/Types/Gameplay-Logica/State-Rule/FiniteStateMachine.h** - Finite state machines
- **Systems/Types/Gameplay-Logica/State-Rule/BehaviorTree.h** - Behavior trees
- **Systems/Types/Gameplay-Logica/State-Rule/UtitlityAI.h** - Utility AI
- **Systems/Types/Gameplay-Logica/State-Rule/GOAPPlans.h** - GOAP planning

### Ability Systems
- **Systems/Types/Gameplay-Logica/Ability-Interactions/SkillActivation.h** - Skill activation
- **Systems/Types/Gameplay-Logica/Ability-Interactions/CoolDownManagement.h** - Cooldown management
- **Systems/Types/Gameplay-Logica/Ability-Interactions/TargetSelection.h** - Target selection
- **Systems/Types/Gameplay-Logica/Ability-Interactions/HitResolution.h** - Hit resolution

### Progression Systems
- **Systems/Types/Gameplay-L极地/Progression-Economy/XPSystem.h** - Experience point systems
- **Systems/Types/Gameplay-Logica/Progression-Economy/Inventory.h** - Inventory management
- **Systems/Types/Gameplay-Logica/Progression-Economy/Crafting.h极地** - Crafting systems
- **Systems/Types/Gameplay-Logica/Progression-Economy/ResourceFlow.h** - Resource flow

### Quest Systems
- **Systems/Types/Gameplay-Logica/Quest-Narrative/ObjectiveTracking.h** - Objective tracking
- **Systems/Types/Gameplay-Logica/Quest-Narrative/TriggerEvaluation.h** - Trigger evaluation
- **Systems/Types/Gameplay-Logica/Quest-Narrative/BranchingLogic.h** - Branching narrative logic
- **Systems/Types/Gameplay-Logica/Quest-Narrative/WorldStateFlags.h** - World state flags

### AI Systems
- **Systems/Types/AI-Cognitive/Perception/VisionCones.h** - Vision cone perception
- **Systems/Types/AI-Cognitive/Perception/AudioPerception极地** - Audio perception
- **Systems/Types/AI-Cognitive/Perception/Line-of-SightTest.h** - Line of sight testing
- **Systems/Types/AI-Cognitive/Perception/ThreatEvaluation.h** - Threat evaluation

### AI Memory
- **Systems/Types/AI-Cognitive/Memory-Knowledge/BlackBoards.h** - Blackboard memory
- **Systems/Types/AI-Cognitive/Memory-Knowledge/World-BeliefsModels.h** - World belief models
- **Systems/Types/AI-Cognitive/Memory-Knowledge/Forgetting-Decay.h** - Forgetting and decay

### AI Learning
- **Systems/Types/AI-Cognitive/Learning-Adaption/ImitationLearning.h** - Imitation learning
- **Systems/Types/AI-Cognitive/Learning-Adaption/RLHooks.h** - Reinforcement learning hooks
- **Systems/Types/AI-Cognitive/Learning-Adaption/PolicyEvaluation.h** - Policy evaluation
- **Systems/Types/AI-Cognitive/Learning-Adaption/RewardShaping.h** - Reward shaping

### Animation Systems
- **Systems/Types/Animation-Motion/Skeletal-Animation/Pose-Sampling.h** - Pose sampling
- **Systems/Types/Animation-Motion/Skeletal-Animation/BlendTrees.h** - Animation blend trees
- **Systems/Types/Animation-Motion/Skeletal-Animation/LayeredAnimation.h** - Layered animation

## ⌨️ Input Module
- **Input/InputEvent.h** - Input event representation and types
- **Input/InputDevice.h** - Base input device interface
- **Input/KeyboardInput.h** - Keyboard input handling
- **Input/MouseInput.h** - Mouse input handling
- **Input/GamepadInput.h** - Gamepad/controller input handling
- **Input/InputBinding.h** - Key binding configuration
- **Input/InputManager.h/cpp** - Central input management

## 🎪 Event Management
- **EventManagement/GameEvent.h** - Base event class interface
- **EventManagement/InputEventTypes.h** - Input-related event types
- **EventManagement/EntityEventTypes.h** - Entity-related event types
- **EventManagement/SceneEventTypes.h** - Scene-related event types
- **EventManagement/PhysicsEventTypes.h** - Physics-related event types
- **EventManagement/EventQueue.h** - Event queuing system
- **EventManagement/EventManager.h** - Event dispatch and subscription

## 🎨 Rendering Engine
### Core Foundation
- **RenderingEngine/Core/Log/log.h** - Rendering-specific logging
- **RenderingEngine/Core/Threading/ThreadUtils.h** - Thread utility functions
- **RenderingEngine/Core/Threading/SpinLock.h** - Spin lock implementation
- **RenderingEngine/Core/Containers/Bitset.h** - Bit set container
- **RenderingEngine/Core/Containers/Hashmap.h** - Hash map container
- **RenderingEngine/Core/Containers/Array.h** - Dynamic array container
- **RenderingEngine/Core/Profiling/profiler.h/cpp** - Performance profiling
- **RenderingEngine/Core/memory/LinearAllocator.h** - Linear memory allocator
- **RenderingEngine/Core/memory/RingAllocator.h** - Ring buffer allocator
- **RenderingEngine/Core/memory/PoolAllocator.h** - Pool allocator
- **RenderingEngine/Core/memory/MemoryTracker.h** - Memory usage tracking
- **RenderingEngine/Core/Threading/Atomic.h** - Atomic operations
- **RenderingEngine/Core/types/ID.h** - ID generation and management
- **RenderingEngine/Core/types/Handle.h** - Resource handle system
- **RenderingEngine/Core/types/Result.h** - Operation result types

### Platform Abstraction
- **RenderingEngine/Platform/platform.h** - Platform abstraction layer
- **RenderingEngine/Platform/Time/Timer.h** - Platform-specific timing
- **RenderingEngine/Platform/Input/Input.h** - Platform input abstraction
- **RenderingEngine/Platform/Input/InputWin32.cpp** - Windows input implementation
- **RenderingEngine/Platform/window/Window.h** - Window management
- **RenderingEngine/Platform/window/WindowWin32.h** - Windows window implementation

### Runtime Systems
- **RenderingEngine/Runtime/Resources/ResourceManager.h** - Resource management
- **RenderingEngine/Runtime/Resources/AssetCache.h** - Asset caching system
- **RenderingEngine/Runtime/Resources/GPUTransfer.h** - GPU data transfer
- **RenderingEngine/Runtime/Resources/UploadQueue.h** - Data upload queue
- **RenderingEngine/Runtime/Visibility/VisibleSet.h** - Visibility set management
- **RenderingEngine/Runtime/Visibility/Occlusion.h** - Occlusion culling
- **RenderingEngine/Runtime/Visibility/LOD.h** - Level of detail management
- **RenderingEngine/Runtime/Visibility/Frustum.h** - View frustum operations
- **RenderingEngine/Runtime/Render_Scene/SceneBuilder.h** - Render scene construction
- **RenderingEngine/Runtime/Render_Scene/Environment.h** - Environment rendering
- **RenderingEngine/Runtime/Render_Scene/MaterialInstance.h** - Material instance management
- **RenderingEngine/Runtime/Render_Scene/RenderView.h** - Render view configuration
- **RenderingEngine/Runtime/Render_Scene/RenderObject.h** - Render object representation
- **RenderingEngine/Runtime/World/World.h** - Runtime world management
- **RenderingEngine/Runtime/World/ComponentStorage.h** - Component storage
- **RenderingEngine/Runtime/World/Entity.h** - Runtime entity representation
- **RenderingEngine/Runtime/World/Transform.h** - Runtime transform system
- **RenderingEngine/Runtime/Job/Fibre.h** - Fiber-based job system
- **RenderingEngine/Runtime/Job/JobSystem.h** - Job scheduling system
- **RenderingEngine/Runtime/Job/Job.h** - Job definition and execution
- **RenderingEngine/Runtime/frame/FrameScheduler.h** - Frame scheduling
- **RenderingEngine/Runtime/frame/FrameContext.h** - Frame execution context
- **RenderingEngine/Runtime/frame/FrameResources.h/cpp** - Per-frame resource management
- **RenderingEngine/Runtime/frame/FrameMetrics.h** - Frame performance metrics
- **RenderingEngine/Runtime/engine/EngineLoop.h/cpp** - Engine main loop

### Frame Graph
- **RenderingEngine/Runtime/frameGraph/GPUCompiler.h** - GPU command compilation
- **RenderingEngine/Runtime/frameGraph/GPUExecutor.h** - GPU command execution
- **RenderingEngine/Runtime/frameGraph/GraphExecutor.h** - Graph execution
- **RenderingEngine/Runtime/frameGraph/GraphCompiler.h** - Graph compilation
- **RenderingEngine/Runtime/frameGraph/极地Validation.h** - Graph validation
- **RenderingEngine/Runtime/frameGraph/PassBuilder.h** - Pass construction
- **RenderingEngine/Runtime/frameGraph/ResourceNode.h** - Resource node management
- **RenderingEngine/Runtime/frameGraph/PassNode.h** - Pass node management
- **RenderingEngine/Runtime/frameGraph/FrameGraph.h** - Frame graph representation

### RHI (Render Hardware Interface)
- **RenderingEngine/rhi/RHI.h** - RHI main interface
- **RenderingEngine/rhi/RHIDev极地** - Device abstraction
- **RenderingEngine/rhi/RHIResource.h** - Resource abstraction
- **RenderingEngine/rhi/RHIBuffer.h** - Buffer resource
- **RenderingEngine/rhi/RHITexture.h** - Texture resource
- **RenderingEngine/rhi/RHISampler.h** - Sampler resource
- **RenderingEngine/rhi/RHIPipeline.h** - Pipeline state
- **RenderingEngine/rhi/RHIFrameBuffer.h** - Framebuffer
- **RenderingEngine/rhi/RHICommand.h** - Command abstraction
- **RenderingEngine/rhi/RHICommandList.h** - Command list
- **RenderingEngine/rhi/RHISync.h** - Synchronization primitives
- **RenderingEngine/rhi/RHIQuery.h** - Query operations
- **RenderingEngine/rhi/RHIResourcePool.h** - Resource pooling

### Vulkan Implementation
- **RenderingEngine/Vulkan/VulkanInstance.cpp** - Vulkan instance management
- **RenderingEngine/Vulkan/VulkanDevice.cpp** - Vulkan device management
- **RenderingEngine/Vulkan/VulkanSwapChain.cpp** - Swap chain management
- **RenderingEngine/Vulkan/VulkanMemory.cpp** - Memory management
- **RenderingEngine/Vulkan/VulkanBuffer.cpp** - Buffer implementation
- **RenderingEngine/Vulkan/VulkanTexture.cpp** - Texture implementation
- **RenderingEngine/Vulkan/VulkanPipeline.cpp** - Pipeline implementation
- **RenderingEngine/Vulkan/VulkanDescriptor.cpp** - Descriptor management
- **RenderingEngine/Vulkan/VulkanCommandList.cpp** - Command list implementation
- **RenderingEngine/Vulkan/VulkanSync.cpp** - Synchronization implementation
- **RenderingEngine/Vulkan/VulkanDebug.cpp** - Debug utilities

### Renderer
- **RenderingEngine/Renderer/renderer.h/cpp** - Main renderer implementation
- **RenderingEngine/Renderer/Passes/PassBase.h** - Base render pass
- **RenderingEngine/Renderer/Passes/DepthPrePass.cpp** - Depth pre-pass
- **RenderingEngine/Renderer/Passes/ShadowPass.cpp** - Shadow pass
- **RenderingEngine/Renderer/Passes/GBufferPass.cpp** - G-buffer pass
- **RenderingEngine/Renderer/Passes/LightingPass.cpp** - Lighting pass
- **极地/Renderer/Passes/SkyPass.cpp** - Sky pass
- **RenderingEngine/Renderer/Passes/PostProcessingPass.cpp** - Post-processing pass
- **RenderingEngine/Renderer/Passes/UIPass.cpp** - UI pass
- **RenderingEngine/Renderer/Passes/DebugPass.cpp** - Debug pass
- **RenderingEngine/Renderer/Techniques/ForwardRenderer.cpp** - Forward rendering
- **RenderingEngine/Renderer/Techniques/DefferedRenderer.cpp** - Deferred rendering
- **RenderingEngine/Renderer/Techniques/ClusteredRender.cpp** - Clustered rendering

### Assets
- **RenderingEngine/Assets/shaders/Compiled_Shader.py** - Shader compilation script

## ⏰ Time Module
- **Time/FrameTimer.h/cpp** - Frame timing and delta time calculation
- **Time/FrameBudget.h/cpp** - Frame time budgeting
- **Time/TimeScale.h/cpp** - Time scaling control
- **Time/TimeManager.h/cpp** - Central time management

## 📦 Components Module
### Existential Components
- **Components/Existential/Identity.h** - Entity identity and metadata
- **Components/Existential/Persistence.h** - Persistence and save state
- **Components/Existential/PrefabInstance.h** - Prefab instance tracking
- **Components/Existential/DestroyOnEvent.h** - Event-based destruction
- **Components/Existential/SpawnMaker.h** - Entity spawning
- **Components/Existential/VersionStamp.h** - Version tracking
- **Components/Existential/LifeTime.h** - Lifetime management
- **Components/Existential/Enable.h** - Enable/disable state

### Spatial Components
- **Components/Spatial/Transform.h** - Transform component
- **Components/Spatial/LocalTransform.h** - Local space transform
- **Components/Spatial/WorldTransform.h** - World space transform
- **Components/Spatial/Parent.h** - Parent relationship
- **Components/Spatial/Children.h** - Children relationship
- **Components/Spatial/Attachment.h** - Attachment points
- **Components/Spatial/BoundingBox,h** - Bounding box volume
- **Components/Spatial/BoundingSphere.h** - Bounding sphere volume
- **Components/Spatial/SpatialSphere.h** - Spatial sphere representation

### Physical Components
- **Components/Physical/Velocity.h** - Linear velocity
- **Components/Physical/Acceleration.h** - Linear acceleration
- **Components/Physical/AngularVelocity,h** - Angular velocity
- **Components/Physical/AngularAcceleration.h** - Angular acceleration
- **Components/Physical/Mass.h** - Mass property
- **Components/Physical/Inertia.h** - Inertia tensor
- **Components/Physical/ForceAccumulator.h** - Force accumulation
- **Components/Physical/RigidBody.h** - Rigid body physics
- **Components/Physical/SoftBody.h** - Soft body physics
- **Components/Physical/Collider.h** - Collision shape
- **Components/Physical/CollisionMask.h** - Collision filtering
- **Components/Physical/PhysicsMaterial.h** - Physics material properties
- **Components/Physical/Constraints.h** - Physics constraints

### Temporal Components
- **Components/Temporal/Timer.h** - Timer component
- **Components/Temporal/CoolDown.h** - Cooldown timer
- **Components/Temporal/AnimationTime.h** - Animation timing
- **Components/Temporal/StateDuration.h** - State duration tracking
- **Components/Temporal/LifeTimeProgress.h** - Lifetime progress
- **Components/Temporal/Aging.h** - Aging system
- **Components/Temporal/TickRateDuration.h** - Tick rate management

### Behavior Components
- **Components/Behavior/InputSource.h** - Input source binding
- **Components/Behavior/AIController.h** - AI controller
- **Components/Behavior/MovementCapability.h** - Movement capabilities
- **Components/Behavior/RotationCapability.h** - Rotation capabilities
- **Components/Behavior/AttackCapability.h** - Attack capabilities
- **Components/Behavior/Interactable.h** - Interactable objects
- **Components/Behavior/Usable.h** - Usable objects
- **Components/Behavior/Contollable.h** - Controllable entities
- **Components/Behavior/NavigationAgent.h** - Navigation agent
- **Components/Behavior/DecisionContext.h** - Decision context

### Perceptual Components
- **Components/Perpectual/RenderMesh.h** - Renderable mesh
- **Components/Perpectual/Material.h** - Material properties
- **Components/Perpectual/Visbility.h** - Visibility state
- **Components/Perpectual/Light.h** - Light source
- **Components/Perpectual/Camera.h** - Camera component
- **Components/Perpectual/AudioSource.h** - Audio source
- **Components/Perpectual/ParticleEmitter.h** - Particle emitter
- **Components/Perpectual/Decal.h** - Decal projection
- **Components/Perpectual/UIAnchor.h** - UI anchor point
- **Components/Perpectual/LODGroup.h** - LOD group management

### Relational Components
- **Components/Relational/Parent.h** - Parent entity reference
- **Components/Relational/Child.h** - Child entity reference
- **Components/Relational/Owner.h** - Ownership relationship
- **Components/Relational/Team.h** - Team affiliation
- **Components/Relational/Faction.h** - Faction alignment
- **Components/Relational/Inventory.h** - Inventory container
- **Components/Relational/Equipment.h** - Equipment slots
- **Components/Relational/Socket.h** - Attachment sockets
- **Components/Relational/Link.h** - Entity linking
-极地/Relational/Target.h** - Target reference

### Logical Components
- **Components/Logical/Health.h** - Health system
- **Components/Logical/Shield.h** - Shield system
- **Components/Logical/Mana.h** - Mana resource
- **Components/Logical/Energy.h** - Energy resource
- **Components/Logical/Score.h** - Scoring system
- **Components/Logical/Level.h** - Level system
- **Components/Logical/Experience.h** - Experience system
- **Components/Logical/QuoteState.h** - State quoting
- **Components/Logical/Objective.h** - Objective tracking
- **Components/Logical/StatusEffect.h** - Status effects
- **Components/Logical/Buff.h** - Buff effects
- **Components/Logical/DeBuff.h** - Debuff effects
- **Components/Logical/DamageSource.h** - Damage source
- **Components/Logical/Threat.h** - Threat level

### Common Components
- **Components/Common/ComponentClass.h** - Component class metadata
- **Components/Common/ComponentTraits.h** - Component type traits
- **Components/Common/ComponentBase.h** - Base component class
- **Components/Common/ComponentMetadata.h** - Component metadata

## 🧪 Test Module
- **Test/SerializationTest.cpp** - Serialization system test suite
- **Test/CMakeLists.txt** - Test build configuration

## 📚 Third-Party Libraries
- **ThirdParty/PhysX/** - NVIDIA PhysX physics engine integration

## 📊 Project Statistics
- **Total Files:** 300+ files
- **Estimated LOC:** 15,000+ lines of code
- **Engine Type:** C++17 Entity Component System
- **Architecture:** Data-oriented design with clean separation
- **Status:** Core systems complete, ready for expansion

This is a comprehensive C++ game engine with ECS architecture, serialization, physics, rendering, and extensive component systems for game development.