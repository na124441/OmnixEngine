#include "Components/Behavior/MovementCapability.h"
#include "../../ECS/ECSComponents.h" // Include the consolidated ECS components header

void MovementCapability::Update(Coordinator& coordinator, float dt)
{
    for (auto const& entity : m_Entities)
    {
        // Get the new TransformComponent and RigidBodyComponent
        auto& transform = coordinator.GetComponent<TransformComponent>(entity);
        auto const& rigidBody = coordinator.GetComponent<RigidBodyComponent>(entity);

        // Update position using rigidBody's velocity
        transform.position.x += rigidBody.velocity.x * dt;
        transform.position.y += rigidBody.velocity.y * dt;
    }
}
