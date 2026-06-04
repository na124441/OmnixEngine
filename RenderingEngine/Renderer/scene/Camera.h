#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace eng::renderer {

struct Camera
{
    glm::vec3 position  = glm::vec3(0.0f, 0.0f, 30.0f);
    glm::vec3 target    = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up        = glm::vec3(0.0f, 1.0f, 0.0f);

    float     fovY      = glm::radians(45.0f);
    float     nearPlane = 0.1f;
    float     farPlane  = 100.0f;

    // -----------------------------------------------------------------
    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(position, target, up);
    }

    glm::mat4 getProjMatrix(float aspectRatio) const
    {
        return glm::perspective(fovY, aspectRatio, nearPlane, farPlane);
    }
};

} // namespace eng::renderer
