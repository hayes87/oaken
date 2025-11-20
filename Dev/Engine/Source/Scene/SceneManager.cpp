#include "SceneManager.h"

namespace Core {
    SceneManager::SceneManager() {}
    SceneManager::~SceneManager() {}

    void SceneManager::LoadScene(std::unique_ptr<Scene> scene) {
        m_ActiveScene = std::move(scene);
        if (m_ActiveScene) {
            m_ActiveScene->Init();
        }
    }

    void SceneManager::Update(double dt) {
        if (m_ActiveScene) {
            m_ActiveScene->Update(dt);
        }
    }
}
