#pragma once
#include "ResourceManager.h"
#include <ozz/animation/runtime/animation.h>

namespace Resources {
    class Animation : public Resource {
    public:
        ozz::animation::Animation animation;
        
        bool Load(const std::string& path);
    };
}
