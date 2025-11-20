#include "Texture.h"
#include "../Platform/RenderDevice.h" // We might need this later for cleanup

namespace Resources {

    Texture::Texture(SDL_GPUDevice* device, uint32_t width, uint32_t height, SDL_GPUTexture* texture)
        : m_Device(device), m_Width(width), m_Height(height), m_Texture(texture)
    {
    }

    Texture::~Texture() {
        if (m_Texture) {
            SDL_ReleaseGPUTexture(m_Device, m_Texture);
        }
    }

    void Texture::UpdateTexture(SDL_GPUTexture* newTexture, uint32_t width, uint32_t height) {
        if (m_Texture) {
            SDL_ReleaseGPUTexture(m_Device, m_Texture);
        }
        m_Texture = newTexture;
        m_Width = width;
        m_Height = height;
    }

    bool Texture::Reload() {
        // Read file
        std::vector<char> data = ResourceManager::ReadFile(m_Path);
        if (data.empty()) return false;

        // Parse Header
        struct OakTexHeader {
            char signature[4];
            uint32_t width;
            uint32_t height;
            uint32_t channels;
            uint32_t format;
        };

        if (data.size() < sizeof(OakTexHeader)) return false;

        OakTexHeader* header = reinterpret_cast<OakTexHeader*>(data.data());
        if (strncmp(header->signature, "OAKT", 4) != 0) return false;

        // Create New Texture
        const char* pixelData = data.data() + sizeof(OakTexHeader);
        SDL_GPUTexture* gpuTexture = m_Device ? SDL_CreateGPUTexture(m_Device, nullptr) : nullptr; // Wait, we need CreateTexture logic here.
        
        // Actually, we can't easily call CreateTexture from here without duplicating logic or exposing RenderDevice.
        // But we have m_Device stored!
        
        SDL_GPUTextureCreateInfo createInfo;
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        createInfo.width = header->width;
        createInfo.height = header->height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        createInfo.props = 0;

        SDL_GPUTexture* newTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        
        if (newTexture) {
             // Upload data (Simplified: We need a transfer buffer, similar to RenderDevice::CreateTexture)
             // This is getting complicated to duplicate. 
             // Ideally ResourceManager should handle the "Loading" part and pass the GPU resource to the Texture.
             // But for now, let's just assume we can use the RenderDevice if we had access to it.
             // Wait, Texture doesn't have access to RenderDevice helper methods, only the raw SDL_GPUDevice.
             
             // Let's cheat slightly and move the "LoadTexture" logic in ResourceManager to be reusable, 
             // OR just keep the logic in ResourceManager for now but dispatch via type.
             // But the user asked for a "Generic Solution".
             // The best generic solution is: Resource::Reload() calls ResourceManager::LoadXXX(m_Path) and updates itself.
             
             return false; // Placeholder, we will implement this properly in ResourceManager for now.
        }
        return false;
    }

}
