#pragma once

#include "ResourceManager.h"
#include <SDL3/SDL.h>

namespace Resources {

    class Shader : public Resource {
    public:
        Shader(SDL_GPUDevice* device, SDL_GPUShader* shader, SDL_GPUShaderStage stage, 
               uint32_t samplers = 0, uint32_t storageTextures = 0, uint32_t storageBuffers = 0, uint32_t uniformBuffers = 1);
        ~Shader();

        bool Reload() override;

        SDL_GPUShader* GetShader() const { return m_Shader; }
        SDL_GPUShaderStage GetStage() const { return m_Stage; }

    private:
        SDL_GPUDevice* m_Device;
        SDL_GPUShader* m_Shader;
        SDL_GPUShaderStage m_Stage;
        
        uint32_t m_NumSamplers;
        uint32_t m_NumStorageTextures;
        uint32_t m_NumStorageBuffers;
        uint32_t m_NumUniformBuffers;
    };

}
