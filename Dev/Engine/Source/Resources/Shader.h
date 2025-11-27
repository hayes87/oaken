#pragma once

#include "ResourceManager.h"
#include <SDL3/SDL.h>
#include <vector>

namespace Resources {

    class Shader : public Resource {
    public:
        Shader(SDL_GPUDevice* device, SDL_GPUShader* shader, SDL_GPUShaderStage stage, 
               uint32_t samplers = 0, uint32_t storageTextures = 0, uint32_t storageBuffers = 0, uint32_t uniformBuffers = 1);
        ~Shader();

        bool Reload() override;

        SDL_GPUShader* GetShader() const { return m_Shader; }
        SDL_GPUShaderStage GetStage() const { return m_Stage; }
        
        // For compute shaders - access raw bytecode
        const uint8_t* GetBytecode() const { return m_Bytecode.data(); }
        size_t GetBytecodeSize() const { return m_Bytecode.size(); }
        bool IsCompute() const { return !m_Bytecode.empty(); }

    private:
        SDL_GPUDevice* m_Device;
        SDL_GPUShader* m_Shader;
        SDL_GPUShaderStage m_Stage;
        
        uint32_t m_NumSamplers;
        uint32_t m_NumStorageTextures;
        uint32_t m_NumStorageBuffers;
        uint32_t m_NumUniformBuffers;
        
        // Store bytecode for compute shaders (needed for pipeline creation)
        std::vector<uint8_t> m_Bytecode;
    };

}
