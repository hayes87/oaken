#include "CharacterSystem.h"
#include "../Components/Components.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Systems {
    CharacterSystem::CharacterSystem(flecs::world& world, Platform::Input& input) {
        Platform::Input* inputPtr = &input;

        // System to handle character movement based on input
        // Movement is relative to the camera's facing direction
        world.system<LocalTransform, CharacterController>("CharacterMovementSystem")
            .run([inputPtr, &world](flecs::iter& it) {
                // Get the camera yaw for movement direction
                float cameraYaw = 0.0f;
                world.query<const CameraFollowComponent, const CameraComponent>()
                    .each([&](flecs::entity e, const CameraFollowComponent& follow, const CameraComponent& cam) {
                        if (cam.isPrimary) {
                            cameraYaw = follow.yaw;
                        }
                    });

                while (it.next()) {
                    auto transforms = it.field<LocalTransform>(0);
                    auto controllers = it.field<CharacterController>(1);

                    float dt = it.delta_time();

                    for (auto i : it) {
                        LocalTransform& transform = transforms[i];
                        CharacterController& controller = controllers[i];

                        // Get input direction
                        glm::vec2 inputDir(0.0f);
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_W)) inputDir.y += 1.0f;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_S)) inputDir.y -= 1.0f;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_A)) inputDir.x -= 1.0f;
                        if (inputPtr->IsKeyDown(SDL_SCANCODE_D)) inputDir.x += 1.0f;

                        bool isRunning = inputPtr->IsKeyDown(SDL_SCANCODE_LSHIFT);
                        bool hasInput = glm::length(inputDir) > 0.01f;

                        // Update state
                        if (hasInput) {
                            controller.state = isRunning ? CharacterState::Running : CharacterState::Walking;
                        } else {
                            controller.state = CharacterState::Idle;
                        }

                        if (hasInput) {
                            // Normalize input
                            inputDir = glm::normalize(inputDir);

                            // Calculate movement direction relative to camera
                            float cameraYawRad = glm::radians(cameraYaw);
                            
                            // Forward is the direction the camera is looking (negative Z in camera space)
                            glm::vec3 forward(sin(cameraYawRad), 0.0f, cos(cameraYawRad));
                            glm::vec3 right(cos(cameraYawRad), 0.0f, -sin(cameraYawRad));

                            // Calculate world-space movement direction
                            glm::vec3 moveDir = forward * inputDir.y + right * inputDir.x;
                            moveDir = glm::normalize(moveDir);

                            // Calculate speed
                            float speed = controller.moveSpeed;
                            if (isRunning) speed *= controller.runMultiplier;

                            // Apply velocity
                            controller.velocity = moveDir * speed;

                            // Calculate target rotation (character faces movement direction)
                            controller.targetYaw = glm::degrees(atan2(moveDir.x, moveDir.z));
                        } else {
                            // Decelerate when no input
                            controller.velocity = glm::vec3(0.0f);
                        }

                        // Apply movement
                        transform.position += controller.velocity * dt;

                        // Smoothly rotate character to face movement direction
                        if (hasInput) {
                            float currentYaw = transform.rotation.y;
                            float targetYaw = controller.targetYaw;

                            // Handle angle wrapping
                            float diff = targetYaw - currentYaw;
                            while (diff > 180.0f) diff -= 360.0f;
                            while (diff < -180.0f) diff += 360.0f;

                            // Smooth rotation
                            float rotationStep = controller.turnSpeed * dt * 60.0f; // Scale by 60 for reasonable feel
                            if (std::abs(diff) < rotationStep) {
                                transform.rotation.y = targetYaw;
                            } else {
                                transform.rotation.y += (diff > 0 ? rotationStep : -rotationStep);
                            }

                            // Normalize rotation
                            while (transform.rotation.y > 180.0f) transform.rotation.y -= 360.0f;
                            while (transform.rotation.y < -180.0f) transform.rotation.y += 360.0f;
                        }
                    }
                }
            });
    }
}
