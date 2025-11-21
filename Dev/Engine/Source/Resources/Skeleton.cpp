#include "Skeleton.h"
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include "../Core/Log.h"

namespace Resources {
    bool Skeleton::Load(const std::string& path) {
        ozz::io::File file(path.c_str(), "rb");
        if (!file.opened()) {
            LOG_ERROR("Failed to open skeleton file: {}", path);
            return false;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Skeleton>()) {
            LOG_ERROR("File is not a skeleton: {}", path);
            return false;
        }
        archive >> skeleton;
        return true;
    }
}
