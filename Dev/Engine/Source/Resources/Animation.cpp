#include "Animation.h"
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include "../Core/Log.h"

namespace Resources {
    bool Animation::Load(const std::string& path) {
        ozz::io::File file(path.c_str(), "rb");
        if (!file.opened()) {
            LOG_ERROR("Failed to open animation file: {}", path);
            return false;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Animation>()) {
            LOG_ERROR("File is not an animation: {}", path);
            return false;
        }
        archive >> animation;
        return true;
    }
}
