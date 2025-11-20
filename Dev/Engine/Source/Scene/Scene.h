#pragma once
#include <flecs.h>
#include <memory>

namespace Core {
    class Scene {
    public:
        Scene();
        ~Scene();

        void Init();
        void Update(double dt);
        
        flecs::world& GetWorld() { return *m_World; }

    private:
        std::unique_ptr<flecs::world> m_World;
    };
}
