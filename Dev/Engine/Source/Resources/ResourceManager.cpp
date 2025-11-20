#include "ResourceManager.h"
#include "Texture.h"
#include "../Platform/RenderDevice.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

namespace Resources {

    ResourceManager::ResourceManager() {
    }

    ResourceManager::~ResourceManager() {
        Shutdown();
    }

    void ResourceManager::Init(Platform::RenderDevice* renderDevice) {
        m_RenderDevice = renderDevice;
        std::cout << "[ResourceManager] Initialized" << std::endl;
    }

    void ResourceManager::Shutdown() {
        m_Resources.clear();
        std::cout << "[ResourceManager] Shutdown" << std::endl;
    }

    std::vector<char> ResourceManager::ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            std::cerr << "[ResourceManager] Failed to open file: " << path << std::endl;
            return {};
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close(); // Explicitly close, though destructor would do it too.

        return buffer;
    }

    std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& path) {
        // Check cache
        if (m_Resources.find(path) != m_Resources.end()) {
            return std::static_pointer_cast<Texture>(m_Resources[path]);
        }

        // Read file
        std::vector<char> data = ReadFile(path);
        if (data.empty()) return nullptr;

        // Parse Header
        struct OakTexHeader {
            char signature[4];
            uint32_t width;
            uint32_t height;
            uint32_t channels;
            uint32_t format;
        };

        if (data.size() < sizeof(OakTexHeader)) {
            std::cerr << "[ResourceManager] File too small: " << path << std::endl;
            return nullptr;
        }

        OakTexHeader* header = reinterpret_cast<OakTexHeader*>(data.data());
        if (strncmp(header->signature, "OAKT", 4) != 0) {
            std::cerr << "[ResourceManager] Invalid texture signature: " << path << std::endl;
            return nullptr;
        }

        // Create Texture
        const char* pixelData = data.data() + sizeof(OakTexHeader);
        SDL_GPUTexture* gpuTexture = m_RenderDevice->CreateTexture(header->width, header->height, pixelData);

        if (!gpuTexture) {
            std::cerr << "[ResourceManager] Failed to create GPU texture: " << path << std::endl;
            return nullptr;
        }

        // We need to pass the device to the texture for cleanup, or handle it differently.
        // For now, let's assume Texture takes ownership of the SDL_GPUTexture* but we need to fix the destructor issue.
        // The Texture class I wrote earlier takes (width, height, texture).
        // But its destructor calls SDL_ReleaseGPUTexture(Platform::RenderDevice::GetDeviceInstance(), m_Texture);
        // I need to fix Texture.cpp to use the device from somewhere.
        
        // Let's update Texture to store the device pointer.
        auto texture = std::make_shared<Texture>(m_RenderDevice->GetDevice(), header->width, header->height, gpuTexture);
        m_Resources[path] = texture;
        
        return texture;
    }

}
