#pragma once
#include "Scene.h"
#include <memory>

namespace Core {
    class SceneManager {
    public:
        SceneManager();
        ~SceneManager();

        void LoadScene(std::unique_ptr<Scene> scene);
        Scene* GetActiveScene() { return m_ActiveScene.get(); }
        
        void Update(double dt);

    private:
        std::unique_ptr<Scene> m_ActiveScene;
    };
}
