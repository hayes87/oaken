#pragma once
#include "Scene.h"
#include "../Resources/ResourceManager.h"
#include <string>

namespace Core {
    class SceneSerializer {
    public:
        SceneSerializer(Scene* scene, Resources::ResourceManager* resourceManager = nullptr);

        void Serialize(const std::string& filepath);
        bool Deserialize(const std::string& filepath);
        bool DeserializeBinary(const std::string& filepath);

    private:
        Scene* m_Scene;
        Resources::ResourceManager* m_ResourceManager;
    };
}
