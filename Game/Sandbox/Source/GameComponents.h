#pragma once

#include <glm/glm.hpp>

struct AttributeSet {
    float health = 100.0f;
    float maxHealth = 100.0f;
    float mana = 100.0f;
    float maxMana = 100.0f;
    float speed = 10.0f;
};

// Component for circular orbit motion (used for point lights)
struct OrbitComponent {
    glm::vec2 center = {0.0f, 0.0f};  // Center point (XZ plane)
    float radius = 2.0f;               // Orbit radius
    float speed = 1.0f;                // Angular speed (radians per second)
    float phase = 0.0f;                // Phase offset
};
