#pragma once

#include <glm/glm.hpp>
#include <string>

struct Transform {
    glm::vec3 position = {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation = {0.0f, 0.0f, 0.0f}; // Euler angles in degrees
    glm::vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct AttributeSet {
    float health = 100.0f;
    float maxHealth = 100.0f;
    float mana = 100.0f;
    float maxMana = 100.0f;
    float speed = 10.0f;
};

struct MeshComponent {
    // Handles will be integers or HashedStrings later
    // For now, just placeholders
    uint32_t meshId = 0;
    uint32_t materialId = 0;
};

struct ScriptComponent {
    std::string scriptName;
};
