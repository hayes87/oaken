#pragma once
#include "ResourceManager.h"
#include <SDL3/SDL.h>

namespace Resources {

    class Texture : public Resource {
    public:
        Texture(SDL_GPUDevice* device, uint32_t width, uint32_t height, SDL_GPUTexture* texture);
        ~Texture();

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }
        SDL_GPUTexture* GetGPUTexture() const { return m_Texture; }

        void UpdateTexture(SDL_GPUTexture* newTexture, uint32_t width, uint32_t height);

    private:
        SDL_GPUDevice* m_Device;
        uint32_t m_Width;
        uint32_t m_Height;
        SDL_GPUTexture* m_Texture;
    };

}
