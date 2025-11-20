#include "Reflection.h"
#include "Components.h"

namespace Components {

    void RegisterReflection(flecs::world& world) {
        
        // Register std::string support
        world.component<std::string>()
            .opaque(flecs::String)
            .serialize([](const flecs::serializer *s, const std::string *data) {
                return s->value(flecs::String, data->c_str());
            })
            .assign_string([](std::string *data, const char *value) {
                *data = value;
            });

        world.component<glm::vec3>()
            .member<float>("x")
            .member<float>("y")
            .member<float>("z");

        world.component<Transform>()
            .member<glm::vec3>("position")
            .member<glm::vec3>("rotation")
            .member<glm::vec3>("scale");

        world.component<MeshComponent>()
            .member<uint32_t>("meshId")
            .member<uint32_t>("materialId");

        world.component<ScriptComponent>()
            .member<std::string>("scriptName");
    }
}
