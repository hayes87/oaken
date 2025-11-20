#include "TransformSystem.h"
#include "../Components/Components.h"
#include <flecs.h>
#include <glm/gtc/matrix_transform.hpp>

namespace Systems {
    TransformSystem::TransformSystem(Core::GameContext& context) : m_Context(context) {}

    void TransformSystem::Init() {
        m_Context.World->system<LocalTransform>("ComputeTransforms")
            .kind(flecs::OnUpdate)
            .each([](flecs::entity e, LocalTransform& local) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, local.position);
                model = glm::rotate(model, glm::radians(local.rotation.x), glm::vec3(1, 0, 0));
                model = glm::rotate(model, glm::radians(local.rotation.y), glm::vec3(0, 1, 0));
                model = glm::rotate(model, glm::radians(local.rotation.z), glm::vec3(0, 0, 1));
                model = glm::scale(model, local.scale);

                // Handle Hierarchy
                flecs::entity parent = e.parent();
                if (parent && parent.has<WorldTransform>()) {
                    const WorldTransform& parentWorld = parent.get<WorldTransform>();
                    model = parentWorld.matrix * model;
                }

                e.set<WorldTransform>({model});
            });
    }
}
