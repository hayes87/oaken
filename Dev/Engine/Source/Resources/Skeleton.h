#pragma once
#include "ResourceManager.h"
#include <ozz/animation/runtime/skeleton.h>

namespace Resources {
    class Skeleton : public Resource {
    public:
        ozz::animation::Skeleton skeleton;
        
        bool Load(const std::string& path);
    };
}
