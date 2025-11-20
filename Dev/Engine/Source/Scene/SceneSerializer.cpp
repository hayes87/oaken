#include "SceneSerializer.h"
#include "../Components/Components.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace Core {
    SceneSerializer::SceneSerializer(Scene* scene) : m_Scene(scene) {}

    void SceneSerializer::Serialize(const std::string& filepath) {
        json root;
        root["scene"] = "Untitled";
        root["entities"] = json::array();

        m_Scene->GetWorld().each([&](flecs::entity e, LocalTransform& t) {
            // Only serialize entities with names for now
            if (!e.name().length()) return;

            json entityJson;
            entityJson["name"] = e.name().c_str();
            
            // Transform
            entityJson["transform"] = {
                {"position", {t.position.x, t.position.y, t.position.z}},
                {"rotation", {t.rotation.x, t.rotation.y, t.rotation.z}},
                {"scale", {t.scale.x, t.scale.y, t.scale.z}}
            };

            // Sprite
            if (e.has<SpriteComponent>()) {
                const SpriteComponent& s = e.get<SpriteComponent>();
                // TODO: Serialize texture path
            }

            root["entities"].push_back(entityJson);
        });

        std::ofstream out(filepath);
        out << root.dump(4);
    }

    bool SceneSerializer::Deserialize(const std::string& filepath) {
        std::ifstream in(filepath);
        if (!in.is_open()) return false;

        json root;
        in >> root;

        auto entities = root["entities"];
        for (auto& entityJson : entities) {
            std::string name = entityJson["name"];
            auto e = m_Scene->GetWorld().entity(name.c_str());

            // Transform
            auto t = entityJson["transform"];
            LocalTransform transform;
            transform.position = {t["position"][0], t["position"][1], t["position"][2]};
            transform.rotation = {t["rotation"][0], t["rotation"][1], t["rotation"][2]};
            transform.scale = {t["scale"][0], t["scale"][1], t["scale"][2]};
            e.set<LocalTransform>(transform);

            // Sprite
            if (entityJson.contains("sprite")) {
                // TODO: Load texture
            }
        }
        return true;
    }
}
