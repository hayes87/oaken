#include "RenderSystem.h"
#include "../Core/Log.h"
#include "../Platform/Window.h"
#include <fstream>
#include <vector>

namespace Systems {

    RenderSystem::RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice)
        : m_Context(context), m_RenderDevice(renderDevice) {}

    RenderSystem::~RenderSystem() {
        if (m_Pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_Pipeline);
        }
    }

    void RenderSystem::Init() {
        CreatePipeline();
    }

    // Helper to load SPIR-V
    static std::vector<uint8_t> LoadShader(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            LOG_CORE_ERROR("Failed to open shader file: {}", filename);
            return {};
        }
        size_t fileSize = (size_t)file.tellg();
        std::vector<uint8_t> buffer(fileSize);
        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    void RenderSystem::CreatePipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;
        SDL_GPUShaderFormat format;

        if (std::string(driver) == "direct3d12") {
            vertPath = "Source/Shaders/Basic.vert.dxil";
            fragPath = "Source/Shaders/Basic.frag.dxil";
            format = SDL_GPU_SHADERFORMAT_DXIL;
            LOG_CORE_INFO("Using D3D12 backend (DXIL shaders)");
        } else {
            vertPath = "Source/Shaders/Basic.vert.spv";
            fragPath = "Source/Shaders/Basic.frag.spv";
            format = SDL_GPU_SHADERFORMAT_SPIRV;
            LOG_CORE_INFO("Using Vulkan/Other backend (SPIR-V shaders)");
        }

        auto vertCode = LoadShader(vertPath);
        auto fragCode = LoadShader(fragPath);

        if (vertCode.empty() || fragCode.empty()) {
            LOG_CORE_ERROR("Failed to load shaders!");
            return;
        }

        SDL_GPUShaderCreateInfo vertInfo = {};
        vertInfo.code_size = vertCode.size();
        vertInfo.code = vertCode.data();
        vertInfo.entrypoint = "main";
        vertInfo.format = format;
        vertInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vertInfo.num_samplers = 0;
        vertInfo.num_storage_textures = 0;
        vertInfo.num_storage_buffers = 0;
        vertInfo.num_uniform_buffers = 0;

        SDL_GPUShaderCreateInfo fragInfo = {};
        fragInfo.code_size = fragCode.size();
        fragInfo.code = fragCode.data();
        fragInfo.entrypoint = "main";
        fragInfo.format = format;
        fragInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fragInfo.num_samplers = 0;
        fragInfo.num_storage_textures = 0;
        fragInfo.num_storage_buffers = 0;
        fragInfo.num_uniform_buffers = 0;

        SDL_GPUShader* vertShader = SDL_CreateGPUShader(device, &vertInfo);
        SDL_GPUShader* fragShader = SDL_CreateGPUShader(device, &fragInfo);

        if (!vertShader || !fragShader) {
            LOG_CORE_ERROR("Failed to create shaders: {}", SDL_GetError());
            return;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader;
        pipelineInfo.fragment_shader = fragShader;
        
        // Input Assembly
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        
        // Rasterizer
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        // Color Blend
        SDL_GPUColorTargetDescription colorTargetDesc = {0};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
        colorTargetDesc.blend_state.enable_blend = false;

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;

        // Depth Stencil (None for now)
        pipelineInfo.depth_stencil_state.enable_depth_test = false;

        m_Pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);

        SDL_ReleaseGPUShader(device, vertShader);
        SDL_ReleaseGPUShader(device, fragShader);

        if (!m_Pipeline) {
            LOG_CORE_ERROR("Failed to create pipeline: {}", SDL_GetError());
        } else {
            LOG_CORE_INFO("Graphics Pipeline Created Successfully!");
        }
    }

    void RenderSystem::BeginFrame() {
        m_RenderDevice.BeginFrame();
    }

    void RenderSystem::DrawScene(double alpha) {
        (void)alpha;
        
        if (!m_Pipeline) return;

        SDL_GPURenderPass* pass = m_RenderDevice.GetRenderPass();
        if (pass) {
            SDL_BindGPUGraphicsPipeline(pass, m_Pipeline);
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        }
    }

    void RenderSystem::EndFrame() {
        m_RenderDevice.EndFrame();
    }

}
