#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include "ThirdParty/imgui/imgui.h"

namespace eng::runtime {

    class EditorCamera {
    public:
        glm::vec3 position = glm::vec3(0.0f, 3.0f, 10.0f);
        float yaw = -90.0f;
        float pitch = -11.3f;
        float fovY = glm::radians(60.0f);
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        float movementSpeed = 5.0f;
        float mouseSensitivity = 0.12f;

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

        bool m_IsDraggingLMB = false;
        bool m_IsDraggingRMB = false;
        bool m_IsDraggingMMB = false;
        float m_OrbitDistance = 10.0f;

        void Update(float deltaTime, bool isViewportHovered, bool isViewportFocused, bool isGizmoActive = false, bool isMouseOverUI = false, const glm::vec3* selectedEntityPosition = nullptr) {
            ImGuiIO& io = ImGui::GetIO();
            bool altDown = io.KeyAlt;

            // Handle transition states for dragging
            // RMB Drag
            if (isViewportHovered && !isMouseOverUI && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                m_IsDraggingRMB = true;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_IsDraggingRMB = false;
            }

            // MMB Drag
            if (isViewportHovered && !isMouseOverUI && ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                m_IsDraggingMMB = true;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Middle) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_IsDraggingMMB = false;
            }

            // LMB Drag
            if (isViewportHovered && !isMouseOverUI && !isGizmoActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                m_IsDraggingLMB = true;
            }
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                m_IsDraggingLMB = false;
            }

            // Calculate camera orbit target/pivot
            glm::vec3 orbitPivot = position + getForward() * m_OrbitDistance;
            if (selectedEntityPosition) {
                orbitPivot = *selectedEntityPosition;
                m_OrbitDistance = glm::distance(position, orbitPivot);
            }

            float deltaX = io.MouseDelta.x;
            float deltaY = io.MouseDelta.y;

            // ALT + Drag Navigation
            if (altDown) {
                if (m_IsDraggingLMB) {
                    // Orbit: rotates camera around the pivot
                    yaw += deltaX * mouseSensitivity;
                    pitch -= deltaY * mouseSensitivity;
                    pitch = std::clamp(pitch, -89.0f, 89.0f);
                    position = orbitPivot - getForward() * m_OrbitDistance;
                }
                else if (m_IsDraggingRMB) {
                    // Dolly/Zoom: moves camera closer to or further from the pivot
                    m_OrbitDistance += deltaY * movementSpeed * 0.02f;
                    m_OrbitDistance = std::max(m_OrbitDistance, 0.1f);
                    position = orbitPivot - getForward() * m_OrbitDistance;
                }
                else if (m_IsDraggingMMB) {
                    // Pan relative to target
                    glm::vec3 panOffset = -getRight() * (deltaX * movementSpeed * 0.005f) + getUp() * (deltaY * movementSpeed * 0.005f);
                    position += panOffset;
                }
            }
            // Standard (Non-ALT) Drag Navigation
            else {
                if (m_IsDraggingLMB) {
                    // LMB Drag: Move forward/backward (deltaY) and rotate yaw (deltaX)
                    yaw += deltaX * mouseSensitivity;
                    
                    // Move along the horizontal (XZ) plane
                    glm::vec3 forward = getForward();
                    forward.y = 0.0f;
                    if (glm::length(forward) > 0.001f) {
                        forward = glm::normalize(forward);
                    }
                    position += forward * (-deltaY * movementSpeed * 0.005f);
                }
                else if (m_IsDraggingRMB) {
                    // RMB Drag: standard fly camera rotate pitch and yaw
                    yaw += deltaX * mouseSensitivity;
                    pitch -= deltaY * mouseSensitivity;
                    pitch = std::clamp(pitch, -89.0f, 89.0f);

                    // Fly controls (WASDQE keys) are active when holding RMB
                    float speedMultiplier = 1.0f;
                    if (io.KeyShift) {
                        speedMultiplier = 3.0f;
                    }
                    float currentSpeed = movementSpeed * speedMultiplier;

                    glm::vec3 forward = getForward();
                    glm::vec3 right = getRight();

                    if (ImGui::IsKeyDown(ImGuiKey_W)) {
                        position += forward * currentSpeed * deltaTime;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_S)) {
                        position -= forward * currentSpeed * deltaTime;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_A)) {
                        position -= right * currentSpeed * deltaTime;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_D)) {
                        position += right * currentSpeed * deltaTime;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_E)) {
                        position += glm::vec3(0.0f, 1.0f, 0.0f) * currentSpeed * deltaTime;
                    }
                    if (ImGui::IsKeyDown(ImGuiKey_Q)) {
                        position -= glm::vec3(0.0f, 1.0f, 0.0f) * currentSpeed * deltaTime;
                    }
                }
                else if (m_IsDraggingMMB) {
                    // MMB Drag: Standard Pan (move left/right/up/down)
                    position -= getRight() * (deltaX * movementSpeed * 0.005f);
                    position += getUp() * (deltaY * movementSpeed * 0.005f);
                }
            }

            // Scroll wheel zooming and speed adjustment
            if (isViewportHovered || m_IsDraggingRMB || m_IsDraggingLMB || m_IsDraggingMMB) {
                if (io.MouseWheel != 0.0f) {
                    if (m_IsDraggingRMB) {
                        // Holding RMB + Scroll Wheel = change camera movement speed
                        movementSpeed += io.MouseWheel * 1.0f;
                        movementSpeed = std::clamp(movementSpeed, 0.5f, 50.0f);
                    } else {
                        // Scroll Wheel alone = move forward/backward (Zoom)
                        position += getForward() * (io.MouseWheel * movementSpeed * 0.5f);
                    }
                }
            }
        }

        void FrameEntity(const glm::vec3& targetPosition, float boundsRadius = 1.0f) {
            float dist = std::max(boundsRadius * 2.5f, 5.0f);
            position = targetPosition - getForward() * dist;
        }
    };

} // namespace eng::runtime
