#include "Shader.h"
#include "../Platform/RenderDevice.h"
#include "../Core/Log.h"

namespace Resources {

    Shader::Shader(SDL_GPUDevice* device, SDL_GPUShader* shader, SDL_GPUShaderStage stage,
                   uint32_t samplers, uint32_t storageTextures, uint32_t storageBuffers, uint32_t uniformBuffers)
        : m_Device(device), m_Shader(shader), m_Stage(stage),
          m_NumSamplers(samplers), m_NumStorageTextures(storageTextures), 
          m_NumStorageBuffers(storageBuffers), m_NumUniformBuffers(uniformBuffers)
    {
    }

    Shader::~Shader() {
        if (m_Shader) {
            SDL_ReleaseGPUShader(m_Device, m_Shader);
        }
    }

    bool Shader::Reload() {
        std::vector<char> data = ResourceManager::ReadFile(m_Path);
        if (data.empty()) return false;

        // For compute shaders, just store bytecode - they're created via SDL_CreateGPUComputePipeline
        // We detect compute shaders by checking if the path contains ".comp"
        if (m_Path.find(".comp") != std::string::npos) {
            m_Bytecode.assign(data.begin(), data.end());
            return true;
        }

        SDL_GPUShaderCreateInfo shaderInfo = {};
        shaderInfo.code_size = data.size();
        shaderInfo.code = (const Uint8*)data.data();
        shaderInfo.entrypoint = "main";
        shaderInfo.stage = m_Stage;
        
        // Determine format based on backend
        const char* driver = SDL_GetGPUDeviceDriver(m_Device);
        if (std::string(driver) == "direct3d12") {
            shaderInfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        } else {
            shaderInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
        }
        
        shaderInfo.num_samplers = m_NumSamplers;
        shaderInfo.num_storage_textures = m_NumStorageTextures;
        shaderInfo.num_storage_buffers = m_NumStorageBuffers;
        shaderInfo.num_uniform_buffers = m_NumUniformBuffers;

        SDL_GPUShader* newShader = SDL_CreateGPUShader(m_Device, &shaderInfo);
        if (!newShader) {
            LOG_CORE_ERROR("Failed to reload shader: {}", m_Path);
            return false;
        }

        if (m_Shader) {
            SDL_ReleaseGPUShader(m_Device, m_Shader);
        }
        m_Shader = newShader;
        
        return true;
    }

}
