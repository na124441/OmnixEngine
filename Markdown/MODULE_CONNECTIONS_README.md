# Omnix Engine - Module Connection Diagram

## 🎯 Main Entry Point: `main.cpp` -> `Core/Application.cpp`

### Primary Execution Flow:
```
main() (Application.cpp:332)
├── Logger::Init()          ← Core/Logger.cpp
├── Platform::Init()        ← Platform abstraction
├── Timer::Init()           ← Core/Timer.cpp
├── World* world = new World() ← Core/World.h
├── StateMachine creation   ← Application.cpp:218
└── Main Game Loop          ← Application.cpp:353
    ├── Platform::PollEvents()
    ├── Timer::Update()
    ├── Input processing
    ├── State transitions
    ├── State::update(dt)
    ├── RenderFrame()
    └── Frame pacing
```

## 🔗 Module Dependency Graph

### Level 1: Core Foundation (Directly called from main)
```
Core/
├── Logger.cpp              ← Logging system (initialized first)
├── Timer.cpp               ← Timing system (frame timing)
├── Application.cpp         ← Main application logic
└── World.h                 ← ECS wrapper and coordinator
```

### Level 2: ECS System (Called via World.h)
```
ECS/
├── Coordinator.h/cpp       ← Central ECS manager
├── EntityManager.h/cpp     ← Entity lifecycle
├── ComponentManager.h/cpp  ← Component storage
├── SystemManager.h/cpp     ← System registration
└── ECSComponents.h         ← Component definitions
```

### Level 3: Component System (Registered via World.h)
```
Components/
├── Existential/            ← Identity, Persistence, LifeTime
├── Spatial/                ← Transforms, Bounding volumes
├── Physical/               ← Physics components
├── Temporal/               ← Timers, Cooldowns
├── Behavior/               ← AI, Input, Navigation
├── Perpectual/             ← Rendering, Audio, Particles
├── Relational/             ← Relationships, Ownership
├── Logical/                ← Game logic, Stats
└── Common/                 ← Base components
```

### Level 4: Systems (Registered and updated via World.h)
```
Systems/
├── Core/                   ← System interfaces
├── Scheduler/              ← Execution scheduling
├── Execution/              ← Update strategies
├── Registry/               ← System registration
└── Types/                  ← Specialized systems
    ├── SimulationSystems/  ← Physics, Particles, Fluids
    ├── SpatialWorldSystems/← Navigation, Streaming
    ├── Gameplay-Logica/    ← Game logic, AI
    └── Animation-Motion/   ← Animation systems
```

### Level 5: Serialization (Called on demand)
```
Serializer/
├── Serialization/          ← Interfaces and common code
├── ECS/                    ← ECS integration
├── Snapshot/               ← State capture
└── Normal/                 ← Binary format implementation
    ├── Binary/             ← Binary serialization
    ├── Text/               ← Text serialization
    └── Delta/              ← Delta compression
```

### Level 6: Scene Management (Called from game states)
```
Scene/
├── Scene.h/cpp             ← Scene container
├── SceneManager.h/cpp      ← Multiple scene management
├── SceneObject.h/cpp       ← Scene entities
├── Transform.h/cpp         ← Transform operations
├── Camera.h/cpp            ← Camera management
├── Prefab.h/cpp            ← Object templates
└── ComponentFactory.h/cpp  ← Component creation
```

### Level 7: Input System (Polled each frame)
```
Input/
├── InputManager.h/cpp      ← Central input handling
├── InputDevice.h           ← Device interface
├── KeyboardInput.h         ← Keyboard input
├── MouseInput.h            ← Mouse input
├── GamepadInput.h          ← Controller input
└── InputBinding.h          ← Key binding configuration
```

### Level 8: Event System (Used throughout engine)
```
EventManagement/
├── EventManager.h          ← Event dispatch
├── EventQueue.h            ← Event queuing
├── GameEvent.h             ← Base event class
├── InputEventTypes.h       ← Input events
├── EntityEventTypes.h      ← Entity events
├── SceneEventTypes.h       ← Scene events
└── PhysicsEventTypes.h     ← Physics events
```

### Level 9: Rendering Engine (Called from RenderFrame())
```
RenderingEngine/
├── Runtime/                ← Runtime systems
├── Core/                   ← Foundation classes
├── Platform/               ← Platform abstraction
├── rhi/                    ← Render hardware interface
├── Vulkan/                 ← Vulkan implementation
├── Renderer/               ← Render passes
└── Assets/                 ← Asset management
```

### Level 10: Time Management (Updated each frame)
```
Time/
├── TimeManager.h/cpp       ← Central time management
├── FrameTimer.h/cpp        ← Frame timing
├── FrameBudget.h/cpp       ← Time budgeting
└── TimeScale.h/cpp         ← Time scaling
```

## 🔄 Execution Order (Per Frame)

1. **Platform Events** - `Platform::PollEvents()`
2. **Time Update** - `Timer::Update()`
3. **Input Processing** - Input system polling
4. **State Transition** - `StateMachine::process_pending()`
5. **Input Handling** - `State::handle_input()`
6. **Game Logic** - `State::update(dt)`
   - ECS system updates
   - Physics simulation
   - AI processing
7. **Rendering** - `RenderFrame()`
   - Visibility culling
   - Render pass execution
   - GPU command submission
8. **Frame Pacing** - `Platform::SleepUntilNextFrame()`

## 🎮 State Machine Integration

```
Game States (Application.cpp)
├── BootState
├── MainMenuState
└── GameplayState
    └── Uses World* for ECS operations
        ├── Entity creation/destruction
        ├── Component manipulation
        ├── System updates
        └── Serialization calls
```

## 💾 Serialization Integration Points

```
Serialization Access Points:
1. Game state transitions (save/load)
2. Network replication
3. Editor undo/redo
4. Debug state capture
5. Prefab instantiation

Called via:
- SerializationBridge::CreateSnapshot()
- NormalSerializer::SerializeToBuffer()
- NormalDeserializer::Deserialize()
```

## 🎨 Rendering Integration

```
RenderFrame() calls:
1. RenderingEngine::Renderer::render()
2. Frame graph execution
3. RHI command submission
4. GPU synchronization

Data Sources:
- ECS render components
- Scene transform hierarchy
- Camera configuration
- Light and material data
```

## 📊 Module Interdependencies

```
Strong Dependencies (→):
Application → Core → ECS → Components
Application → Core → World → Systems
Application → Input → EventManagement
Application → Time → Core

Weak Dependencies (⇢):
ECS ⇢ Serialization (on demand)
Systems ⇢ Physics (optional)
Scene ⇢ ECS (entity management)
Rendering ⇢ Scene (transform hierarchy)

Optional Dependencies:
Physics systems
Network systems
Audio systems
```

## 🔧 Initialization Sequence

```
Phase 0: Pre-init
  - Logger setup
  - Platform detection

Phase 1: Core systems
  - Timer initialization
  - Memory allocators
  - Thread pools

Phase 2: ECS foundation
  - Component registration
  - System registration
  - Entity manager setup

Phase 3: Subsystems
  - Input device detection
  - Render context creation
  - Audio context setup

Phase 4: Content loading
  - Scene loading
  - Asset loading
  - Prefab instantiation

Phase 5: Game ready
  - State machine initialization
  - First state activation
```

## 🚀 Extension Points

### New Module Integration:
1. **Add components** → Register in `Components/` and include in `World.h`
2. **Add systems** → Implement in `Systems/` and register in `World.h`
3. **Add serialization** → Implement `ISerializer`/`IDeserializer` interfaces
4. **Add rendering** → Implement render passes in `RenderingEngine/Renderer/`
5. **Add input** → Implement `InputDevice` interface

### Custom Game States:
1. Inherit from `IGameState`
2. Implement state logic
3. Add to `StateMachine::create_state()`
4. Handle ECS operations through `World*`

This architecture provides clean separation while maintaining efficient communication between all engine modules through the central ECS system and well-defined interfaces.