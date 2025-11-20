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

}
