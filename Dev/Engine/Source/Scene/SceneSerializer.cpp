#include "SceneSerializer.h"
#include "../Components/Components.h"
#include "../Resources/ResourceManager.h"
#include "../Resources/Mesh.h"
#include "../Resources/Texture.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cstdint>

using json = nlohmann::json;

namespace Core {
    SceneSerializer::SceneSerializer(Scene* scene, Resources::ResourceManager* resourceManager) 
        : m_Scene(scene), m_ResourceManager(resourceManager) {}

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
                if (s.texture) {
                    entityJson["sprite"] = {
                        {"texture", s.texture->GetPath()}
                    };
                }
            }

            // Mesh
            if (e.has<MeshComponent>()) {
                const MeshComponent& m = e.get<MeshComponent>();
                if (m.mesh) {
                    entityJson["mesh"] = {
                        {"path", m.mesh->GetPath()}
                    };
                }
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

            // Mesh
            if (entityJson.contains("mesh")) {
                auto meshJson = entityJson["mesh"];
                std::string path = meshJson.value("path", "");
                if (!path.empty() && m_ResourceManager) {
                    auto mesh = m_ResourceManager->LoadMesh(path);
                    e.set<MeshComponent>({mesh});
                }
            }
        }
        return true;
    }

    struct OakLevelHeader {
        char signature[4];
        uint32_t version;
        uint32_t entityCount;
    };

    bool SceneSerializer::DeserializeBinary(const std::string& filepath) {
        std::ifstream in(filepath, std::ios::binary);
        if (!in.is_open()) return false;

        OakLevelHeader header;
        in.read(reinterpret_cast<char*>(&header), sizeof(OakLevelHeader));

        if (strncmp(header.signature, "OAKL", 4) != 0) {
            std::cerr << "Invalid scene file signature" << std::endl;
            return false;
        }

        for (uint32_t i = 0; i < header.entityCount; i++) {
            // Name
            uint32_t nameLen;
            in.read(reinterpret_cast<char*>(&nameLen), sizeof(uint32_t));
            
            std::string name(nameLen, '\0');
            in.read(&name[0], nameLen);

            auto e = m_Scene->GetWorld().entity(name.c_str());

            // Transform
            bool hasTransform;
            in.read(reinterpret_cast<char*>(&hasTransform), sizeof(bool));
            if (hasTransform) {
                LocalTransform transform;
                in.read(reinterpret_cast<char*>(&transform), sizeof(LocalTransform));
                e.set<LocalTransform>(transform);
            }

            // Sprite
            bool hasSprite;
            in.read(reinterpret_cast<char*>(&hasSprite), sizeof(bool));
            if (hasSprite) {
                // TODO: Load sprite data
            }

            // Mesh
            bool hasMesh;
            in.read(reinterpret_cast<char*>(&hasMesh), sizeof(bool));
            if (hasMesh) {
                uint32_t pathLen;
                in.read(reinterpret_cast<char*>(&pathLen), sizeof(uint32_t));
                std::string path(pathLen, '\0');
                in.read(&path[0], pathLen);
                
                if (m_ResourceManager) {
                    auto mesh = m_ResourceManager->LoadMesh(path);
                    e.set<MeshComponent>({mesh});
                }
            }
        }

        return true;
    }
}
