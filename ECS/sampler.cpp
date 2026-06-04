#include <iostream>
#include <thread>
#include <chrono>
#include "Coordinator.h"
#include "../Core/Timer.h"
#include "../Core/Logger.h"
#include "ECSComponents.h" // Include the consolidated ECS components header

// Systems
class MovementSystem : public System {
public:
    void Update(Coordinator& coordinator, float dt) {
        for (auto entity : m_Entities) {
            // Use the new components
            auto& transform = coordinator.GetComponent<TransformComponent>(entity);
            auto& rigidBody = coordinator.GetComponent<RigidBodyComponent>(entity);

            transform.position.x += rigidBody.velocity.x * dt;
            transform.position.y += rigidBody.velocity.y * dt;
            LOG_INFO(("Entity " + std::to_string(entity) +
                         " Position: (" + std::to_string(transform.position.x) + ", " + std::to_string(transform.position.y) + ")").c_str());
        }
    }
};

int main() {
    Coordinator coordinator;
    coordinator.Init();
    Timer::Init(); // Initialize Timer

    // Register components
    coordinator.RegisterComponent<TransformComponent>(); // Use new component
    coordinator.RegisterComponent<RigidBodyComponent>(); // Use new component

    // Register and setup system
    auto movementSystem = coordinator.RegisterSystem<MovementSystem>();
    Signature movementSig;
    movementSig.set(coordinator.GetComponentType<TransformComponent>()); // Use new component
    movementSig.set(coordinator.GetComponentType<RigidBodyComponent>()); // Use new component
    coordinator.SetSystemSignature<MovementSystem>(movementSig);

    int numEntities;
    std::cout << "Enter number of entities: ";
    std::cin >> numEntities;

    // Create entities and add components
    for (int i = 0; i < numEntities; ++i) {
        Entity e = coordinator.CreateEntity();
        float x, y, vx, vy; // Changed dx/dy to vx/vy for clarity with RigidBodyComponent
        std::cout << "Entity " << e << " initial position (x y): ";
        std::cin >> x >> y;
        std::cout << "Entity " << e << " velocity (vx vy): ";
        std::cin >> vx >> vy;

        // Correctly initialize TransformComponent and RigidBodyComponent
        TransformComponent transform;
        transform.position.x = x;
        transform.position.y = y;
        coordinator.AddComponent<TransformComponent>(e, transform);

        RigidBodyComponent rigidBody;
        rigidBody.velocity.x = vx;
        rigidBody.velocity.y = vy;
        coordinator.AddComponent<RigidBodyComponent>(e, rigidBody);
    }

    std::cout << "Starting simulation...\n";

    // Simulation loop
    int frameCount = 0;
    while (frameCount < 10) { // Run 10 frames for demo
        Timer::Update();
        float dt = static_cast<float>(Timer::GetDeltaSeconds());
        movementSystem->Update(coordinator, dt);
        frameCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // slow it down for demo
    }

    std::cout << "Simulation ended.\n";
    return 0;
}
