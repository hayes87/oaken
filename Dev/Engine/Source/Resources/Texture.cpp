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
            // Upload data using transfer buffer
            uint32_t size = header->width * header->height * 4; // RGBA8

            SDL_GPUTransferBufferCreateInfo transferInfo = {};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = size;

            SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferInfo);

            if (transferBuffer) {
                void* map = SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false);
                if (map) {
                    SDL_memcpy(map, pixelData, size);
                    SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);

                    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);
                    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

                    SDL_GPUTextureTransferInfo source = {};
                    source.transfer_buffer = transferBuffer;
                    source.offset = 0;
                    source.pixels_per_row = header->width;
                    source.rows_per_layer = header->height;

                    SDL_GPUTextureRegion destination = {};
                    destination.texture = newTexture;
                    destination.w = header->width;
                    destination.h = header->height;
                    destination.d = 1;

                    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
                    SDL_EndGPUCopyPass(copyPass);
                    SDL_SubmitGPUCommandBuffer(cmd);

                    SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);

                    // Success! Update the texture object
                    UpdateTexture(newTexture, header->width, header->height);
                    return true;
                }
                SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
            }
            SDL_ReleaseGPUTexture(m_Device, newTexture);
        }
        return false;
    }

}
