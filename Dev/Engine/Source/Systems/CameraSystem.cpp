#include "CameraSystem.h"
#include "../Components/Components.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Systems {
    CameraSystem::CameraSystem(flecs::world& world, Platform::Input& input) {
        Platform::Input* inputPtr = &input;

        // Enable mouse capture
        inputPtr->SetRelativeMouseMode(true);

        // System for third-person orbit cameras (CameraFollowComponent)
        world.system<LocalTransform, CameraFollowComponent, const CameraComponent>("CameraFollowSystem")
            .run([inputPtr](flecs::iter& it) {
                float mouseX, mouseY;
                inputPtr->GetMouseDelta(mouseX, mouseY);
                float scrollDelta = inputPtr->GetScrollDelta();

                while (it.next()) {
                    auto transforms = it.field<LocalTransform>(0);
                    auto follows = it.field<CameraFollowComponent>(1);
                    auto cameras = it.field<const CameraComponent>(2);

                    for (auto i : it) {
                        if (!cameras[i].isPrimary) continue;

                        CameraFollowComponent& follow = follows[i];
                        LocalTransform& transform = transforms[i];

                        // Update yaw/pitch based on mouse input
                        follow.yaw -= mouseX * follow.sensitivity;
                        follow.pitch -= mouseY * follow.sensitivity;
                        follow.pitch = std::clamp(follow.pitch, follow.minPitch, follow.maxPitch);

                        // Update distance based on scroll wheel
                        follow.distance -= scrollDelta * follow.zoomSpeed;
                        follow.distance = std::clamp(follow.distance, follow.minDistance, follow.maxDistance);

                        // Get target position
                        glm::vec3 targetPos = follow.offset; // Default if no target
                        if (follow.target.is_valid()) {
                            // Lookup the target entity from the world
                            flecs::entity targetEntity = it.world().entity(follow.target);
                            if (targetEntity.has<LocalTransform>()) {
                                const LocalTransform& targetTransform = targetEntity.get<LocalTransform>();
                                targetPos = targetTransform.position + follow.offset;
                            }
                        }

                        // Calculate camera position using spherical coordinates
                        float pitchRad = glm::radians(follow.pitch);
                        float yawRad = glm::radians(follow.yaw);

                        glm::vec3 cameraOffset;
                        cameraOffset.x = follow.distance * cos(pitchRad) * sin(yawRad);
                        cameraOffset.y = follow.distance * sin(pitchRad);
                        cameraOffset.z = follow.distance * cos(pitchRad) * cos(yawRad);

                        transform.position = targetPos + cameraOffset;

                        // Calculate rotation to look at target
                        glm::vec3 direction = glm::normalize(targetPos - transform.position);
                        transform.rotation.y = glm::degrees(atan2(-direction.x, -direction.z));
                        transform.rotation.x = glm::degrees(asin(direction.y));
                        transform.rotation.z = 0.0f;
                    }
                }
            });

        // System for free-fly cameras (no CameraFollowComponent)
        world.system<LocalTransform, const CameraComponent>("CameraFreeFlightSystem")
            .without<CameraFollowComponent>()
            .run([inputPtr](flecs::iter& it) {
                float mouseX, mouseY;
                inputPtr->GetMouseDelta(mouseX, mouseY);

                while (it.next()) {
                    auto transforms = it.field<LocalTransform>(0);
                    auto cameras = it.field<const CameraComponent>(1);

                    float dt = it.delta_time();
                    float speed = 5.0f * dt;
                    float sensitivity = 0.1f;

                    for (auto i : it) {
                        if (!cameras[i].isPrimary) continue;

                        LocalTransform& transform = transforms[i];

                        // Rotation (Mouse)
                        transform.rotation.y -= mouseX * sensitivity;
                        transform.rotation.x -= mouseY * sensitivity;

                        // Clamp pitch
                        transform.rotation.x = std::clamp(transform.rotation.x, -89.0f, 89.0f);

                        // Movement
                        glm::vec3 forward = glm::vec3(0, 0, -1);
                        glm::vec3 right = glm::vec3(1, 0, 0);
                        glm::vec3 up = glm::vec3(0, 1, 0);

                        // Calculate forward/right based on rotation (Y-up)
                        glm::mat4 rot = glm::rotate(glm::mat4(1.0f), glm::radians(transform.rotation.y), glm::vec3(0, 1, 0));
                        forward = glm::vec3(rot * glm::vec4(forward, 0.0f));
                        right = glm::vec3(rot * glm::vec4(right, 0.0f));

                        if (inputPtr->IsKeyDown(SDL_SCANCODE_W)) transform.position += forward * speed;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_S)) transform.position -= forward * speed;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_A)) transform.position -= right * speed;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_D)) transform.position += right * speed;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_E)) transform.position += up * speed;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_Q)) transform.position -= up * speed;
                    }
                }
            });
    }
}
