#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    class EditorCamera {
    public:
        glm::vec3 position = glm::vec3(0.0f, 4.0f, 10.0f);
        float yaw = -90.0f;
        float pitch = -22.0f;
        float fovY = glm::radians(60.0f);
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        float movementSpeed = 10.0f;
        float mouseSensitivity = 0.15f;

        glm::vec3 getForward() const {
            glm::vec3 forward;
            forward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
            forward.y = sin(glm::radians(pitch));
            forward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
            return glm::normalize(forward);
        }

        glm::vec3 getRight() const {
            return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
        }

        glm::vec3 getUp() const {
            return glm::normalize(glm::cross(getRight(), getForward()));
        }

        glm::mat4 getViewMatrix() const {
            return glm::lookAt(position, position + getForward(), glm::vec3(0.0f, 1.0f, 0.0f));
        }

        glm::mat4 getProjMatrix(float aspectRatio) const {
            return glm::perspective(fovY, aspectRatio, nearPlane, farPlane);
        }

        void LookAt(const glm::vec3& target) {
            glm::vec3 dir = target - position;
            float dist = glm::length(dir);
            if (dist > 0.001f) {
                dir = glm::normalize(dir);
                pitch = glm::degrees(asin(dir.y));
                yaw = glm::degrees(atan2(dir.z, dir.x));
                pitch = std::clamp(pitch, -89.0f, 89.0f);
            }
        }

        void Update(float deltaTime, bool isViewportHovered, bool isViewportFocused) {
            ImGuiIO& io = ImGui::GetIO();

            // Right-click drag mouse look
            if (isViewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                float deltaX = io.MouseDelta.x;
                float deltaY = io.MouseDelta.y;

                yaw += deltaX * mouseSensitivity;
                pitch -= deltaY * mouseSensitivity;
                pitch = std::clamp(pitch, -89.0f, 89.0f);
            }

            // Fly controls when holding Right Mouse Button
            if (isViewportHovered && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                glm::vec3 forward = getForward();
                glm::vec3 right = getRight();

                if (ImGui::IsKeyDown(ImGuiKey_W)) {
                    position += forward * movementSpeed * deltaTime;
                }
                if (ImGui::IsKeyDown(ImGuiKey_S)) {
                    position -= forward * movementSpeed * deltaTime;
                }
                if (ImGui::IsKeyDown(ImGuiKey_A)) {
                    position -= right * movementSpeed * deltaTime;
                }
                if (ImGui::IsKeyDown(ImGuiKey_D)) {
                    position += right * movementSpeed * deltaTime;
                }
                if (ImGui::IsKeyDown(ImGuiKey_E)) {
                    position += glm::vec3(0.0f, 1.0f, 0.0f) * movementSpeed * deltaTime;
                }
                if (ImGui::IsKeyDown(ImGuiKey_Q)) {
                    position -= glm::vec3(0.0f, 1.0f, 0.0f) * movementSpeed * deltaTime;
                }
            }

            // Adjust speed with mouse scroll wheel if viewport hovered
            if (isViewportHovered) {
                if (io.MouseWheel != 0.0f) {
                    movementSpeed += io.MouseWheel * 2.0f;
                    movementSpeed = std::max(1.0f, movementSpeed);
                }
            }
        }
    };

} // namespace eng::runtime
