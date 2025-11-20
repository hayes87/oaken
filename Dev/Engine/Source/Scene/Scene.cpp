#include "Scene.h"
#include "../Components/Components.h"
#include "../Components/Reflection.h"

namespace Core {
    Scene::Scene() {
        m_World = std::make_unique<flecs::world>();
    }

    Scene::~Scene() {
        m_World.reset();
    }

    void Scene::Init() {
        // Register Components
        Components::RegisterReflection(*m_World);
    }

    void Scene::Update(double dt) {
        m_World->progress((float)dt);
    }
}
