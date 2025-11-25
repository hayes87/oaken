#include "CameraSystem.h"
#include "../Components/Components.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Systems {
    CameraSystem::CameraSystem(flecs::world& world, Platform::Input& input) {
        Platform::Input* inputPtr = &input;

        // Enable mouse capture
        inputPtr->SetRelativeMouseMode(true);

        world.system<LocalTransform, const CameraComponent>("CameraSystem")
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
