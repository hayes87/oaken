#include "RenderSystem.h"
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>
#include <iostream>
#include <cmath>
#include "../Components/Components.h"
#include "../Core/Log.h"
#include "../Resources/Mesh.h"
#include "../Resources/Texture.h"
#include "../Resources/Shader.h"
#include "../Resources/Skeleton.h"
#include "../Platform/Window.h"

namespace Systems {

    RenderSystem::RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice, Resources::ResourceManager& resourceManager)
        : m_Context(context), m_RenderDevice(renderDevice), m_ResourceManager(resourceManager) {}

    RenderSystem::~RenderSystem() {
        if (m_Pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_Pipeline);
        }
        if (m_MeshPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_MeshPipeline);
        }
        if (m_InstancedMeshPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_InstancedMeshPipeline);
        }
        if (m_ForwardPlusPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_ForwardPlusPipeline);
        }
        if (m_LinePipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_LinePipeline);
        }
        if (m_ToneMappingPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_ToneMappingPipeline);
        }
        if (m_BloomBrightPassPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_BloomBrightPassPipeline);
        }
        if (m_BloomBlurPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_BloomBlurPipeline);
        }
        if (m_BloomCompositePipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_BloomCompositePipeline);
        }
        if (m_SSGIPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_SSGIPipeline);
        }
        if (m_SSGITemporalPipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_SSGITemporalPipeline);
        }
        if (m_SSGIDenoisePipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_SSGIDenoisePipeline);
        }
        if (m_SSGICompositePipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_SSGICompositePipeline);
        }
        if (m_Sampler) {
            SDL_ReleaseGPUSampler(m_RenderDevice.GetDevice(), m_Sampler);
        }
        if (m_LinearSampler) {
            SDL_ReleaseGPUSampler(m_RenderDevice.GetDevice(), m_LinearSampler);
        }
        if (m_DefaultSkinBuffer) {
            SDL_ReleaseGPUBuffer(m_RenderDevice.GetDevice(), m_DefaultSkinBuffer);
        }
        if (m_InstanceBuffer) {
            SDL_ReleaseGPUBuffer(m_RenderDevice.GetDevice(), m_InstanceBuffer);
        }
    }

    void RenderSystem::Init() {
        CreatePipeline();
        CreateMeshPipeline();
        CreateInstancedMeshPipeline();
        CreateLinePipeline();
        CreateToneMappingPipeline();
        CreateBloomPipelines();
        CreateDepthOnlyPipeline();
        CreateLightCullingPipeline();
        CreateForwardPlusPipeline();
        CreateShadowMapPipeline();
        CreateShadowMapSkinnedPipeline();
        CreateSSGIPipelines();
        
        // Nearest neighbor sampler (for sprites/pixel art)
        SDL_GPUSamplerCreateInfo samplerInfo = {};
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        
        m_Sampler = SDL_CreateGPUSampler(m_RenderDevice.GetDevice(), &samplerInfo);
        
        // Linear sampler (for HDR texture sampling)
        SDL_GPUSamplerCreateInfo linearSamplerInfo = {};
        linearSamplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        linearSamplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        linearSamplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
        linearSamplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        linearSamplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        linearSamplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        
        m_LinearSampler = SDL_CreateGPUSampler(m_RenderDevice.GetDevice(), &linearSamplerInfo);
    }

    void RenderSystem::CreatePipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;
        SDL_GPUShaderFormat format;

        // Use Assets/Shaders/ path (relative to executable/working dir)
        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/Basic.vert.dxil";
            fragPath = "Assets/Shaders/Basic.frag.dxil";
            format = SDL_GPU_SHADERFORMAT_DXIL;
            LOG_CORE_INFO("Using D3D12 backend (DXIL shaders)");
        } else {
            vertPath = "Assets/Shaders/Basic.vert.spv";
            fragPath = "Assets/Shaders/Basic.frag.spv";
            format = SDL_GPU_SHADERFORMAT_SPIRV;
            LOG_CORE_INFO("Using Vulkan/Other backend (SPIR-V shaders)");
        }

        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 0);

        if (!vertShader || !fragShader) {
            LOG_CORE_ERROR("Failed to load shaders!");
            return;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        // Input Assembly
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
        
        // Rasterizer
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        // Color Blend - Use HDR format if HDR is enabled
        SDL_GPUColorTargetDescription colorTargetDesc = {0};
        if (m_RenderDevice.IsHDREnabled()) {
            colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format
        } else {
            colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
        }
        colorTargetDesc.blend_state.enable_blend = true;
        colorTargetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTargetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTargetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colorTargetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colorTargetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colorTargetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        
        // Depth Stencil
        pipelineInfo.target_info.has_depth_stencil_target = false; // Basic pipeline is 2D/Sprite

        m_Pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_Pipeline) {
            LOG_CORE_ERROR("Failed to create graphics pipeline!");
        } else {
            LOG_CORE_INFO("Graphics Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateMeshPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;

        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/Mesh.vert.dxil";
            fragPath = "Assets/Shaders/Mesh.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/Mesh.vert.spv";
            fragPath = "Assets/Shaders/Mesh.frag.spv";
        }

        // Vertex Shader: 0 Samplers, 0 Storage Textures, 0 Storage Buffers, 2 Uniform Buffers (Scene + Skin)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 2);
        // Fragment Shader: 1 Sampler (shadow map), 0 Storage Textures, 0 Storage Buffers, 1 Uniform Buffer (Lights)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);

        if (!vertShader || !fragShader) {
            LOG_CORE_ERROR("Failed to load mesh shaders!");
            return;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        // Vertex Input State
        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.pitch = sizeof(Resources::Vertex);
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesc.instance_step_rate = 0;

        SDL_GPUVertexAttribute vertexAttributes[5];
        // Position
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(Resources::Vertex, position);
        
        // Normal
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(Resources::Vertex, normal);

        // UV
        vertexAttributes[2].location = 2;
        vertexAttributes[2].buffer_slot = 0;
        vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttributes[2].offset = offsetof(Resources::Vertex, uv);

        // Weights
        vertexAttributes[3].location = 3;
        vertexAttributes[3].buffer_slot = 0;
        vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[3].offset = offsetof(Resources::Vertex, weights);

        // Joints - using FLOAT4 to read as float, shader will convert
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);

        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesc;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 5;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

        // Color Target - Use HDR format if HDR is enabled
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        if (m_RenderDevice.IsHDREnabled()) {
            colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format
        } else {
            colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
        }
        colorTargetDesc.blend_state.enable_blend = false; // Opaque for now

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        
        // Depth Stencil
        SDL_GPUDepthStencilState depthStencilState = {};
        depthStencilState.enable_depth_test = true;
        depthStencilState.enable_depth_write = true;
        depthStencilState.compare_op = SDL_GPU_COMPAREOP_LESS;
        
        pipelineInfo.depth_stencil_state = depthStencilState;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

        m_MeshPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_MeshPipeline) {
            LOG_CORE_ERROR("Failed to create mesh pipeline!");
        } else {
            LOG_CORE_INFO("Mesh Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateInstancedMeshPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;

        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/MeshInstanced.vert.dxil";
            fragPath = "Assets/Shaders/MeshInstanced.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/MeshInstanced.vert.spv";
            fragPath = "Assets/Shaders/MeshInstanced.frag.spv";
        }

        // Vertex Shader: 0 Samplers, 0 Storage Textures, 0 Storage Buffers, 1 Uniform Buffer (ViewProj)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        // Fragment Shader: 0 Samplers, 0 Storage Textures, 0 Storage Buffers, 1 Uniform Buffer (Lights)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 1);

        if (!vertShader || !fragShader) {
            LOG_CORE_WARN("Failed to load instanced mesh shaders - batching will be disabled");
            return;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        // Vertex Input State - Two vertex buffers: mesh vertices + instance data
        SDL_GPUVertexBufferDescription vertexBufferDescs[2] = {};
        
        // Buffer 0: Mesh vertex data (per-vertex)
        vertexBufferDescs[0].slot = 0;
        vertexBufferDescs[0].pitch = sizeof(Resources::Vertex);
        vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescs[0].instance_step_rate = 0;
        
        // Buffer 1: Instance data (per-instance)
        vertexBufferDescs[1].slot = 1;
        vertexBufferDescs[1].pitch = sizeof(MeshInstance);  // mat4 + vec4 = 80 bytes
        vertexBufferDescs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vertexBufferDescs[1].instance_step_rate = 0;  // Must be 0 per SDL_GPU spec

        SDL_GPUVertexAttribute vertexAttributes[10];
        
        // Per-vertex attributes (from buffer 0)
        // Position
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(Resources::Vertex, position);
        
        // Normal
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(Resources::Vertex, normal);

        // UV
        vertexAttributes[2].location = 2;
        vertexAttributes[2].buffer_slot = 0;
        vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttributes[2].offset = offsetof(Resources::Vertex, uv);

        // Weights (unused for static but shader expects it)
        vertexAttributes[3].location = 3;
        vertexAttributes[3].buffer_slot = 0;
        vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[3].offset = offsetof(Resources::Vertex, weights);

        // Joints (unused for static but shader expects it)
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);

        // Per-instance attributes (from buffer 1)
        // Model matrix (mat4 = 4 x vec4, locations 5-8)
        vertexAttributes[5].location = 5;
        vertexAttributes[5].buffer_slot = 1;
        vertexAttributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[5].offset = 0;  // Column 0

        vertexAttributes[6].location = 6;
        vertexAttributes[6].buffer_slot = 1;
        vertexAttributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[6].offset = 16;  // Column 1

        vertexAttributes[7].location = 7;
        vertexAttributes[7].buffer_slot = 1;
        vertexAttributes[7].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[7].offset = 32;  // Column 2

        vertexAttributes[8].location = 8;
        vertexAttributes[8].buffer_slot = 1;
        vertexAttributes[8].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[8].offset = 48;  // Column 3

        // Instance color (location 9)
        vertexAttributes[9].location = 9;
        vertexAttributes[9].buffer_slot = 1;
        vertexAttributes[9].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[9].offset = 64;  // After mat4

        pipelineInfo.vertex_input_state.num_vertex_buffers = 2;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescs;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 10;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

        // Color Target - Use HDR format if HDR is enabled
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        if (m_RenderDevice.IsHDREnabled()) {
            colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format
        } else {
            colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
        }
        colorTargetDesc.blend_state.enable_blend = false; // Opaque

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        
        // Depth Stencil
        SDL_GPUDepthStencilState depthStencilState = {};
        depthStencilState.enable_depth_test = true;
        depthStencilState.enable_depth_write = true;
        depthStencilState.compare_op = SDL_GPU_COMPAREOP_LESS;
        
        pipelineInfo.depth_stencil_state = depthStencilState;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

        m_InstancedMeshPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_InstancedMeshPipeline) {
            LOG_CORE_WARN("Failed to create instanced mesh pipeline - batching will be disabled");
        } else {
            LOG_CORE_INFO("Instanced Mesh Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateLinePipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;

        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/Line.vert.dxil";
            fragPath = "Assets/Shaders/Line.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/Line.vert.spv";
            fragPath = "Assets/Shaders/Line.frag.spv";
        }

        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);

        if (!vertShader || !fragShader) {
            LOG_CORE_ERROR("Failed to load line shaders!");
            return;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        // Vertex Input State
        SDL_GPUVertexBufferDescription vertexBufferDesc = {};
        vertexBufferDesc.slot = 0;
        vertexBufferDesc.pitch = sizeof(LineVertex);
        vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDesc.instance_step_rate = 0;

        SDL_GPUVertexAttribute vertexAttributes[2];
        // Position
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(LineVertex, position);
        
        // Color
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(LineVertex, color);

        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesc;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 2;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

        // Input Assembly
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;

        // Rasterizer
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

        // Color Target - Use HDR format if HDR is enabled
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        if (m_RenderDevice.IsHDREnabled()) {
            colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format
        } else {
            colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
        }
        colorTargetDesc.blend_state.enable_blend = false;

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        
        // Depth Stencil
        SDL_GPUDepthStencilState depthStencilState = {};
        depthStencilState.enable_depth_test = true;
        depthStencilState.enable_depth_write = false; // Don't write depth for debug lines usually, or maybe yes? Let's say false for now to see them through transparent stuff? No, true is better for occlusion.
        // Actually, let's disable depth test to see skeleton inside mesh
        depthStencilState.enable_depth_test = false; 
        depthStencilState.compare_op = SDL_GPU_COMPAREOP_ALWAYS;
        
        pipelineInfo.depth_stencil_state = depthStencilState;
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

        m_LinePipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_LinePipeline) {
            LOG_CORE_ERROR("Failed to create line pipeline!");
        } else {
            LOG_CORE_INFO("Line Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateToneMappingPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;
        
        // Use Assets/Shaders/ path (relative to executable/working dir)
        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/Fullscreen.vert.dxil";
            fragPath = "Assets/Shaders/ToneMapping.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/Fullscreen.vert.spv";
            fragPath = "Assets/Shaders/ToneMapping.frag.spv";
        }
        
        // Fullscreen vertex shader: no vertex input, 0 uniform buffers
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
        // ToneMapping fragment shader: 2 samplers (HDR + bloom), 1 uniform buffer (params)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0, 0, 1);
        
        if (!vertShader || !fragShader) {
            LOG_CORE_ERROR("Failed to load tone mapping shaders!");
            return;
        }
        
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        // No vertex input (fullscreen triangle generated in shader)
        pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
        
        // Triangle list
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        
        // Rasterizer
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        
        // Color Target - outputs to swapchain format
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
        colorTargetDesc.blend_state.enable_blend = false;
        
        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        
        // No depth testing for fullscreen pass
        pipelineInfo.target_info.has_depth_stencil_target = false;
        
        m_ToneMappingPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_ToneMappingPipeline) {
            LOG_CORE_ERROR("Failed to create tone mapping pipeline!");
        } else {
            LOG_CORE_INFO("Tone Mapping Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateBloomPipelines() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        bool isD3D12 = std::string(driver) == "direct3d12";
        
        // ========== Bright Pass Pipeline ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/BrightPass.frag.dxil" : "Assets/Shaders/BrightPass.frag.spv";
            
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load bright pass shaders - Bloom will be unavailable");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format
                colorTargetDesc.blend_state.enable_blend = false;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_BloomBrightPassPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_BloomBrightPassPipeline) {
                    LOG_CORE_INFO("Bloom Bright Pass Pipeline Created Successfully!");
                }
            }
        }
        
        // ========== Blur Pipeline ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/BloomBlur.frag.dxil" : "Assets/Shaders/BloomBlur.frag.spv";
            
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load blur shaders - Bloom will be unavailable");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                colorTargetDesc.blend_state.enable_blend = false;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_BloomBlurPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_BloomBlurPipeline) {
                    LOG_CORE_INFO("Bloom Blur Pipeline Created Successfully!");
                }
            }
        }
        
        // ========== Composite Pipeline ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/BloomComposite.frag.dxil" : "Assets/Shaders/BloomComposite.frag.spv";
            
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            // Composite shader: 2 samplers (scene + bloom), 1 uniform buffer
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load bloom composite shaders - Bloom will be unavailable");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                // Composite outputs to HDR texture (for further tone mapping)
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                colorTargetDesc.blend_state.enable_blend = false;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_BloomCompositePipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_BloomCompositePipeline) {
                    LOG_CORE_INFO("Bloom Composite Pipeline Created Successfully!");
                }
            }
        }
    }

    void RenderSystem::CreateSSGIPipelines() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        bool isD3D12 = std::string(driver) == "direct3d12";
        
        // ========== SSGI Main Pipeline (Ray March) ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/SSGI.frag.dxil" : "Assets/Shaders/SSGI.frag.spv";
            
            // Fullscreen vertex: 0 samplers, 0 storage textures, 0 storage buffers, 0 uniform buffers
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            // SSGI fragment: 4 samplers (color, depth, normal, noise), 0 storage, 1 uniform buffer
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 4, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load SSGI shaders - SSGI will be unavailable");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                colorTargetDesc.blend_state.enable_blend = false;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_SSGIPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_SSGIPipeline) {
                    LOG_CORE_INFO("SSGI Pipeline Created Successfully!");
                }
            }
        }
        
        // ========== SSGI Temporal Accumulation Pipeline ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/SSGITemporal.frag.dxil" : "Assets/Shaders/SSGITemporal.frag.spv";
            
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            // Temporal: 4 samplers (current, history, depth, velocity), 1 uniform buffer
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 4, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load SSGI Temporal shaders");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                colorTargetDesc.blend_state.enable_blend = false;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_SSGITemporalPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_SSGITemporalPipeline) {
                    LOG_CORE_INFO("SSGI Temporal Pipeline Created Successfully!");
                }
            }
        }
        
        // ========== SSGI Denoise Pipeline ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/SSGIDenoise.frag.dxil" : "Assets/Shaders/SSGIDenoise.frag.spv";
            
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            // Denoise: 3 samplers (GI, depth, normal), 1 uniform buffer
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 3, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load SSGI Denoise shaders");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                colorTargetDesc.blend_state.enable_blend = false;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_SSGIDenoisePipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_SSGIDenoisePipeline) {
                    LOG_CORE_INFO("SSGI Denoise Pipeline Created Successfully!");
                }
            }
        }
        
        // ========== SSGI Composite Pipeline ==========
        {
            std::string vertPath = isD3D12 ? "Assets/Shaders/Fullscreen.vert.dxil" : "Assets/Shaders/Fullscreen.vert.spv";
            std::string fragPath = isD3D12 ? "Assets/Shaders/SSGIComposite.frag.dxil" : "Assets/Shaders/SSGIComposite.frag.spv";
            
            auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 0);
            // Composite: 2 samplers (scene, GI), 1 uniform buffer
            auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0, 0, 1);
            
            if (!vertShader || !fragShader) {
                LOG_CORE_WARN("Failed to load SSGI Composite shaders");
            } else {
                SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
                pipelineInfo.vertex_shader = vertShader->GetShader();
                pipelineInfo.fragment_shader = fragShader->GetShader();
                pipelineInfo.vertex_input_state.num_vertex_buffers = 0;
                pipelineInfo.vertex_input_state.num_vertex_attributes = 0;
                pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
                pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
                pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
                
                // Output to HDR texture with additive blending
                SDL_GPUColorTargetDescription colorTargetDesc = {};
                colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
                colorTargetDesc.blend_state.enable_blend = true;
                colorTargetDesc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                colorTargetDesc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                colorTargetDesc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
                colorTargetDesc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                colorTargetDesc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
                colorTargetDesc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
                pipelineInfo.target_info.num_color_targets = 1;
                pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
                pipelineInfo.target_info.has_depth_stencil_target = false;
                
                m_SSGICompositePipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
                if (m_SSGICompositePipeline) {
                    LOG_CORE_INFO("SSGI Composite Pipeline Created Successfully!");
                }
            }
        }
    }

    void RenderSystem::CreateDepthOnlyPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;
        
        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/DepthOnly.vert.dxil";
            fragPath = "Assets/Shaders/DepthOnly.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/DepthOnly.vert.spv";
            fragPath = "Assets/Shaders/DepthOnly.frag.spv";
        }
        
        // Depth-only vertex shader: same as instanced mesh but only writes depth
        // 1 uniform buffer (ViewProjection at set 1, binding 0)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        // Empty fragment shader (no resources)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
        
        if (!vertShader || !fragShader) {
            LOG_CORE_WARN("Failed to load depth-only shaders - Forward+ will be unavailable");
            return;
        }
        
        // Same vertex layout as instanced mesh shader
        SDL_GPUVertexBufferDescription vertexBufferDescs[2] = {};
        
        // Buffer 0: Mesh vertex data (per-vertex) - must match Resources::Vertex
        vertexBufferDescs[0].slot = 0;
        vertexBufferDescs[0].pitch = sizeof(Resources::Vertex);
        vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescs[0].instance_step_rate = 0;
        
        // Buffer 1: Instance data (per-instance) - mat4 + vec4 = 80 bytes
        vertexBufferDescs[1].slot = 1;
        vertexBufferDescs[1].pitch = sizeof(MeshInstance);
        vertexBufferDescs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vertexBufferDescs[1].instance_step_rate = 0;  // Must be 0 per SDL_GPU spec
        
        SDL_GPUVertexAttribute vertexAttributes[10] = {};
        
        // Per-vertex attributes (from buffer 0)
        // Position
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(Resources::Vertex, position);
        
        // Normal
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(Resources::Vertex, normal);

        // UV
        vertexAttributes[2].location = 2;
        vertexAttributes[2].buffer_slot = 0;
        vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttributes[2].offset = offsetof(Resources::Vertex, uv);

        // Weights (unused for depth pass but shader expects it)
        vertexAttributes[3].location = 3;
        vertexAttributes[3].buffer_slot = 0;
        vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[3].offset = offsetof(Resources::Vertex, weights);

        // Joints (unused for depth pass but shader expects it)
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);

        // Per-instance attributes (from buffer 1)
        // Model matrix (mat4 = 4 x vec4, locations 5-8)
        vertexAttributes[5].location = 5;
        vertexAttributes[5].buffer_slot = 1;
        vertexAttributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[5].offset = 0;  // Column 0

        vertexAttributes[6].location = 6;
        vertexAttributes[6].buffer_slot = 1;
        vertexAttributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[6].offset = 16;  // Column 1

        vertexAttributes[7].location = 7;
        vertexAttributes[7].buffer_slot = 1;
        vertexAttributes[7].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[7].offset = 32;  // Column 2

        vertexAttributes[8].location = 8;
        vertexAttributes[8].buffer_slot = 1;
        vertexAttributes[8].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[8].offset = 48;  // Column 3

        // Instance color (location 9)
        vertexAttributes[9].location = 9;
        vertexAttributes[9].buffer_slot = 1;
        vertexAttributes[9].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[9].offset = 64;  // After mat4
        
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        pipelineInfo.vertex_input_state.num_vertex_buffers = 2;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescs;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 10;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;
        
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        
        // No color target for depth-only pass
        pipelineInfo.target_info.num_color_targets = 0;
        pipelineInfo.target_info.color_target_descriptions = nullptr;
        
        // Depth buffer configuration
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        
        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        
        m_DepthOnlyPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_DepthOnlyPipeline) {
            LOG_CORE_WARN("Failed to create depth-only pipeline - Forward+ will be unavailable");
        } else {
            LOG_CORE_INFO("Depth-Only Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateLightCullingPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string compPath;
        
        if (std::string(driver) == "direct3d12") {
            compPath = "Assets/Shaders/LightCulling.comp.dxil";
        } else {
            compPath = "Assets/Shaders/LightCulling.comp.spv";
        }
        
        // For compute shaders, we just load the bytecode directly (not via LoadShader)
        std::vector<char> bytecode = Resources::ResourceManager::ReadFile(compPath);
        if (bytecode.empty()) {
            LOG_CORE_WARN("Failed to load light culling compute shader - Forward+ will be unavailable");
            return;
        }
        
        SDL_GPUComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.code = reinterpret_cast<const Uint8*>(bytecode.data());
        pipelineInfo.code_size = bytecode.size();
        pipelineInfo.entrypoint = "main";
        pipelineInfo.format = (std::string(driver) == "direct3d12") ? SDL_GPU_SHADERFORMAT_DXIL : SDL_GPU_SHADERFORMAT_SPIRV;
        pipelineInfo.num_samplers = 1;
        pipelineInfo.num_readonly_storage_textures = 0;
        pipelineInfo.num_readonly_storage_buffers = 1;  // Light buffer
        pipelineInfo.num_readwrite_storage_textures = 0;
        pipelineInfo.num_readwrite_storage_buffers = 1; // Tile light indices
        pipelineInfo.num_uniform_buffers = 1;           // View data
        pipelineInfo.threadcount_x = 16;
        pipelineInfo.threadcount_y = 16;
        pipelineInfo.threadcount_z = 1;
        
        m_LightCullingPipeline = SDL_CreateGPUComputePipeline(device, &pipelineInfo);
        if (!m_LightCullingPipeline) {
            LOG_CORE_WARN("Failed to create light culling compute pipeline: {} - Forward+ will be unavailable", SDL_GetError());
        } else {
            LOG_CORE_INFO("Light Culling Compute Pipeline Created Successfully!");
            
            // Create depth sampler for compute shader
            SDL_GPUSamplerCreateInfo samplerInfo = {};
            samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
            samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
            samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            
            m_DepthSampler = SDL_CreateGPUSampler(device, &samplerInfo);
        }
    }

    void RenderSystem::CreateShadowMapPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;
        
        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/ShadowMap.vert.dxil";
            fragPath = "Assets/Shaders/ShadowMap.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/ShadowMap.vert.spv";
            fragPath = "Assets/Shaders/ShadowMap.frag.spv";
        }
        
        // Vertex shader: 1 uniform buffer (LightSpaceMatrix at set 1, binding 0)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        // Fragment shader: no resources (depth-only output)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
        
        if (!vertShader || !fragShader) {
            LOG_CORE_WARN("Failed to load shadow map shaders - shadows will be unavailable");
            return;
        }
        
        // Same vertex layout as instanced mesh (we reuse instance buffer)
        SDL_GPUVertexBufferDescription vertexBufferDescs[2] = {};
        
        // Buffer 0: Mesh vertex data
        vertexBufferDescs[0].slot = 0;
        vertexBufferDescs[0].pitch = sizeof(Resources::Vertex);
        vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescs[0].instance_step_rate = 0;
        
        // Buffer 1: Instance data
        vertexBufferDescs[1].slot = 1;
        vertexBufferDescs[1].pitch = sizeof(MeshInstance);
        vertexBufferDescs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vertexBufferDescs[1].instance_step_rate = 0;
        
        SDL_GPUVertexAttribute vertexAttributes[10] = {};
        
        // Per-vertex: position (location 0)
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(Resources::Vertex, position);
        
        // Normal (location 1) - unused but layout must match
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(Resources::Vertex, normal);

        // UV (location 2)
        vertexAttributes[2].location = 2;
        vertexAttributes[2].buffer_slot = 0;
        vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttributes[2].offset = offsetof(Resources::Vertex, uv);

        // Weights (location 3)
        vertexAttributes[3].location = 3;
        vertexAttributes[3].buffer_slot = 0;
        vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[3].offset = offsetof(Resources::Vertex, weights);

        // Joints (location 4)
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);

        // Per-instance: Model matrix (locations 5-8)
        vertexAttributes[5].location = 5;
        vertexAttributes[5].buffer_slot = 1;
        vertexAttributes[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[5].offset = 0;

        vertexAttributes[6].location = 6;
        vertexAttributes[6].buffer_slot = 1;
        vertexAttributes[6].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[6].offset = 16;

        vertexAttributes[7].location = 7;
        vertexAttributes[7].buffer_slot = 1;
        vertexAttributes[7].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[7].offset = 32;

        vertexAttributes[8].location = 8;
        vertexAttributes[8].buffer_slot = 1;
        vertexAttributes[8].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[8].offset = 48;

        // Instance color (location 9)
        vertexAttributes[9].location = 9;
        vertexAttributes[9].buffer_slot = 1;
        vertexAttributes[9].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[9].offset = 64;
        
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        pipelineInfo.vertex_input_state.num_vertex_buffers = 2;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescs;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 10;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;
        
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        
        // Back-face culling for shadow map (use normal culling, apply bias for acne)
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        
        // Depth bias to reduce shadow acne
        pipelineInfo.rasterizer_state.enable_depth_bias = true;
        pipelineInfo.rasterizer_state.depth_bias_constant_factor = 1.25f;
        pipelineInfo.rasterizer_state.depth_bias_slope_factor = 1.75f;
        
        // No color targets (depth-only)
        pipelineInfo.target_info.num_color_targets = 0;
        pipelineInfo.target_info.color_target_descriptions = nullptr;
        
        // Depth target - D32F for shadow map precision
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        
        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        
        m_ShadowMapPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_ShadowMapPipeline) {
            LOG_CORE_WARN("Failed to create shadow map pipeline: {} - shadows will be unavailable", SDL_GetError());
        } else {
            LOG_CORE_INFO("Shadow Map Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateShadowMapSkinnedPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        LOG_CORE_INFO("Creating Skinned Shadow Map Pipeline...");
        
        std::string vertPath, fragPath;
        
        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/ShadowMapSkinned.vert.dxil";
            fragPath = "Assets/Shaders/ShadowMap.frag.dxil";  // Reuse depth-only fragment shader
        } else {
            vertPath = "Assets/Shaders/ShadowMapSkinned.vert.spv";
            fragPath = "Assets/Shaders/ShadowMap.frag.spv";
        }
        
        LOG_CORE_INFO("Loading skinned shadow shaders: {} and {}", vertPath, fragPath);
        
        // Vertex shader: 3 uniform buffers (LightSpace, Model, Skin matrices)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 3);
        // Fragment shader: no resources (depth-only output)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);
        
        if (!vertShader || !fragShader) {
            LOG_CORE_WARN("Failed to load skinned shadow map shaders - vertShader:{} fragShader:{}", 
                          vertShader ? "OK" : "NULL", fragShader ? "OK" : "NULL");
            return;
        }
        
        // Vertex layout for non-instanced skinned mesh
        SDL_GPUVertexBufferDescription vertexBufferDescs[1] = {};
        
        // Buffer 0: Mesh vertex data only (no instance buffer for skinned)
        vertexBufferDescs[0].slot = 0;
        vertexBufferDescs[0].pitch = sizeof(Resources::Vertex);
        vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescs[0].instance_step_rate = 0;
        
        SDL_GPUVertexAttribute vertexAttributes[5] = {};
        
        // Per-vertex: position (location 0)
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(Resources::Vertex, position);
        
        // Normal (location 1)
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(Resources::Vertex, normal);

        // UV (location 2)
        vertexAttributes[2].location = 2;
        vertexAttributes[2].buffer_slot = 0;
        vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttributes[2].offset = offsetof(Resources::Vertex, uv);

        // Weights (location 3)
        vertexAttributes[3].location = 3;
        vertexAttributes[3].buffer_slot = 0;
        vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[3].offset = offsetof(Resources::Vertex, weights);

        // Joints (location 4)
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);
        
        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescs;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 5;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;
        
        pipelineInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        
        // Back-face culling for shadow map
        pipelineInfo.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pipelineInfo.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_BACK;
        pipelineInfo.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        
        // Depth bias
        pipelineInfo.rasterizer_state.enable_depth_bias = true;
        pipelineInfo.rasterizer_state.depth_bias_constant_factor = 1.25f;
        pipelineInfo.rasterizer_state.depth_bias_slope_factor = 1.75f;
        
        // No color targets (depth-only)
        pipelineInfo.target_info.num_color_targets = 0;
        pipelineInfo.target_info.color_target_descriptions = nullptr;
        
        // Depth target
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        
        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        
        m_ShadowMapSkinnedPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_ShadowMapSkinnedPipeline) {
            LOG_CORE_WARN("Failed to create skinned shadow map pipeline: {} - skinned mesh shadows unavailable", SDL_GetError());
        } else {
            LOG_CORE_INFO("Skinned Shadow Map Pipeline Created Successfully!");
        }
    }

    void RenderSystem::CreateForwardPlusPipeline() {
        SDL_GPUDevice* device = m_RenderDevice.GetDevice();
        const char* driver = SDL_GetGPUDeviceDriver(device);
        
        std::string vertPath, fragPath;

        if (std::string(driver) == "direct3d12") {
            vertPath = "Assets/Shaders/MeshInstanced.vert.dxil";
            fragPath = "Assets/Shaders/MeshInstancedForwardPlus.frag.dxil";
        } else {
            vertPath = "Assets/Shaders/MeshInstanced.vert.spv";
            fragPath = "Assets/Shaders/MeshInstancedForwardPlus.frag.spv";
        }

        // Vertex Shader: 0 Samplers, 0 Storage Textures, 0 Storage Buffers, 1 Uniform Buffer (ViewProj)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        // Fragment Shader: 1 Sampler (shadow map), 0 Storage Textures, 2 Storage Buffers (lights + tile indices), 1 Uniform Buffer
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0, 2, 1);

        if (!vertShader || !fragShader) {
            LOG_CORE_WARN("Failed to load Forward+ shaders - Forward+ rendering will be unavailable");
            return;
        }

        SDL_GPUGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.vertex_shader = vertShader->GetShader();
        pipelineInfo.fragment_shader = fragShader->GetShader();
        
        // Same vertex layout as instanced mesh pipeline
        SDL_GPUVertexBufferDescription vertexBufferDescs[2] = {};
        
        vertexBufferDescs[0].slot = 0;
        vertexBufferDescs[0].pitch = sizeof(Resources::Vertex);
        vertexBufferDescs[0].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vertexBufferDescs[0].instance_step_rate = 0;
        
        vertexBufferDescs[1].slot = 1;
        vertexBufferDescs[1].pitch = sizeof(MeshInstance);
        vertexBufferDescs[1].input_rate = SDL_GPU_VERTEXINPUTRATE_INSTANCE;
        vertexBufferDescs[1].instance_step_rate = 0;

        SDL_GPUVertexAttribute vertexAttributes[10];
        
        // Position
        vertexAttributes[0].location = 0;
        vertexAttributes[0].buffer_slot = 0;
        vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[0].offset = offsetof(Resources::Vertex, position);
        
        // Normal
        vertexAttributes[1].location = 1;
        vertexAttributes[1].buffer_slot = 0;
        vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        vertexAttributes[1].offset = offsetof(Resources::Vertex, normal);

        // UV
        vertexAttributes[2].location = 2;
        vertexAttributes[2].buffer_slot = 0;
        vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        vertexAttributes[2].offset = offsetof(Resources::Vertex, uv);

        // Weights
        vertexAttributes[3].location = 3;
        vertexAttributes[3].buffer_slot = 0;
        vertexAttributes[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[3].offset = offsetof(Resources::Vertex, weights);

        // Joints
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);

        // Model matrix (4 x vec4)
        for (int i = 0; i < 4; i++) {
            vertexAttributes[5 + i].location = 5 + i;
            vertexAttributes[5 + i].buffer_slot = 1;
            vertexAttributes[5 + i].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
            vertexAttributes[5 + i].offset = i * 16;
        }

        // Instance color
        vertexAttributes[9].location = 9;
        vertexAttributes[9].buffer_slot = 1;
        vertexAttributes[9].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        vertexAttributes[9].offset = 64;

        pipelineInfo.vertex_input_state.num_vertex_buffers = 2;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = vertexBufferDescs;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 10;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

        // Color Target - HDR format
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        colorTargetDesc.blend_state.enable_blend = false;

        pipelineInfo.target_info.num_color_targets = 1;
        pipelineInfo.target_info.color_target_descriptions = &colorTargetDesc;
        
        // Depth Stencil
        pipelineInfo.depth_stencil_state.enable_depth_test = true;
        pipelineInfo.depth_stencil_state.enable_depth_write = true;
        pipelineInfo.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        
        pipelineInfo.target_info.has_depth_stencil_target = true;
        pipelineInfo.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

        m_ForwardPlusPipeline = SDL_CreateGPUGraphicsPipeline(device, &pipelineInfo);
        if (!m_ForwardPlusPipeline) {
            LOG_CORE_WARN("Failed to create Forward+ pipeline: {}", SDL_GetError());
        } else {
            LOG_CORE_INFO("Forward+ Pipeline Created Successfully!");
        }
    }

    void RenderSystem::RenderDepthPrePass(const glm::mat4& view, const glm::mat4& proj) {
        if (!m_DepthOnlyPipeline || !m_RenderDevice.IsForwardPlusEnabled()) return;
        
        if (!m_RenderDevice.BeginDepthPrePass()) return;
        
        SDL_GPURenderPass* pass = m_RenderDevice.GetRenderPass();
        SDL_BindGPUGraphicsPipeline(pass, m_DepthOnlyPipeline);
        
        // Set viewport
        SDL_GPUViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = static_cast<float>(m_RenderDevice.GetRenderWidth());
        viewport.h = static_cast<float>(m_RenderDevice.GetRenderHeight());
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        SDL_SetGPUViewport(pass, &viewport);
        
        // Push ViewProjection uniform
        struct ViewProj {
            glm::mat4 view;
            glm::mat4 proj;
        } vp;
        vp.view = view;
        vp.proj = proj;
        SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &vp, sizeof(vp));
        
        // Render all batches (depth only)
        for (auto& [meshPtr, batch] : m_Batches) {
            if (batch.instances.empty()) continue;
            
            auto mesh = batch.mesh;
            if (!mesh) continue;
            
            SDL_GPUBufferBinding vertexBuffers[2] = {};
            vertexBuffers[0].buffer = mesh->GetVertexBuffer();
            vertexBuffers[0].offset = 0;
            vertexBuffers[1].buffer = m_InstanceBuffer;
            vertexBuffers[1].offset = batch.instanceOffset * sizeof(Systems::MeshInstance);
            
            SDL_BindGPUVertexBuffers(pass, 0, vertexBuffers, 2);
            
            SDL_GPUBufferBinding indexBufferBinding = {};
            indexBufferBinding.buffer = mesh->GetIndexBuffer();
            indexBufferBinding.offset = 0;
            SDL_BindGPUIndexBuffer(pass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            
            SDL_DrawGPUIndexedPrimitives(pass, mesh->GetIndexCount(), static_cast<uint32_t>(batch.instances.size()), 0, 0, 0);
        }
        
        m_RenderDevice.EndRenderPass();
    }

    void RenderSystem::RenderShadowPass(const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData) {
        if (!m_ShadowMapPipeline || !m_RenderDevice.IsShadowsEnabled()) return;
        if (!m_RenderDevice.IsFrameValid()) return;
        
        // Calculate light space matrix from directional light
        glm::vec3 lightDir = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.3f));  // Default
        
        // Query for directional light to get actual direction
        m_Context.World->query<const DirectionalLight>()
            .each([&](flecs::entity e, const DirectionalLight& light) {
                lightDir = glm::normalize(light.direction);
            });
        
        // Get camera position and direction for shadow frustum placement
        glm::vec3 cameraPos = glm::vec3(0.0f);
        glm::vec3 cameraLookAt = glm::vec3(0.0f);
        bool foundCamera = false;
        
        // Try to get camera with follow component (has currentLookAt which is better)
        m_Context.World->query<LocalTransform, const CameraComponent, const CameraFollowComponent>()
            .each([&](flecs::entity e, LocalTransform& t, const CameraComponent& cam, const CameraFollowComponent& follow) {
                if (cam.isPrimary) {
                    cameraPos = t.position;
                    cameraLookAt = follow.currentLookAt;
                    foundCamera = true;
                }
            });
        
        // Fallback: use just camera transform
        if (!foundCamera) {
            m_Context.World->query<LocalTransform, const CameraComponent>()
                .each([&](flecs::entity e, LocalTransform& t, const CameraComponent& cam) {
                    if (cam.isPrimary) {
                        cameraPos = t.position;
                        // Default look direction
                        float yaw = glm::radians(t.rotation.y);
                        cameraLookAt = cameraPos + glm::vec3(sin(yaw), 0.0f, -cos(yaw)) * 10.0f;
                        foundCamera = true;
                    }
                });
        }
        
        // Shadow frustum parameters - cover the whole play area
        float shadowDistance = 100.0f;
        float shadowNear = 1.0f;        
        float shadowFar = 200.0f;       
        float orthoSize = 80.0f;        // Large area
        
        // Center shadow frustum at where camera is looking (on ground level)
        glm::vec3 shadowCenter = glm::vec3(cameraLookAt.x, 0.0f, cameraLookAt.z);
        
        // Light position: go OPPOSITE to light direction (light shines from lightPos toward shadowCenter)
        glm::vec3 lightPos = shadowCenter - lightDir * shadowDistance;
        glm::mat4 lightView = glm::lookAt(lightPos, shadowCenter, glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Orthographic projection - use ZO (zero-to-one) for Vulkan's depth range
        glm::mat4 lightProj = glm::orthoZO(-orthoSize, orthoSize, -orthoSize, orthoSize, shadowNear, shadowFar);
        
        // Store for use in main pass
        m_LightSpaceMatrix = lightProj * lightView;
        
        // Begin shadow render pass
        if (!m_RenderDevice.BeginShadowPass()) return;
        
        SDL_GPURenderPass* pass = m_RenderDevice.GetRenderPass();
        SDL_BindGPUGraphicsPipeline(pass, m_ShadowMapPipeline);
        
        // Set viewport to shadow map size
        uint32_t shadowSize = m_RenderDevice.GetShadowMapSize();
        SDL_GPUViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = static_cast<float>(shadowSize);
        viewport.h = static_cast<float>(shadowSize);
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        SDL_SetGPUViewport(pass, &viewport);
        
        // Push light space matrix uniform
        SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &m_LightSpaceMatrix, sizeof(m_LightSpaceMatrix));
        
        // Render all batches to shadow map
        for (auto& [meshPtr, batch] : m_Batches) {
            if (batch.instances.empty()) continue;
            
            auto mesh = batch.mesh;
            if (!mesh) continue;
            
            SDL_GPUBufferBinding vertexBuffers[2] = {};
            vertexBuffers[0].buffer = mesh->GetVertexBuffer();
            vertexBuffers[0].offset = 0;
            vertexBuffers[1].buffer = m_InstanceBuffer;
            vertexBuffers[1].offset = batch.instanceOffset * sizeof(Systems::MeshInstance);
            
            SDL_BindGPUVertexBuffers(pass, 0, vertexBuffers, 2);
            
            SDL_GPUBufferBinding indexBufferBinding = {};
            indexBufferBinding.buffer = mesh->GetIndexBuffer();
            indexBufferBinding.offset = 0;
            SDL_BindGPUIndexBuffer(pass, &indexBufferBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            
            SDL_DrawGPUIndexedPrimitives(pass, mesh->GetIndexCount(), static_cast<uint32_t>(batch.instances.size()), 0, 0, 0);
            m_Stats.drawCalls++;
        }
        
        // Render skinned meshes to shadow map
        RenderSkinnedMeshesToShadowMap(pass, skinData);
        
        m_RenderDevice.EndShadowPass();
    }
    
    void RenderSystem::RenderSkinnedMeshesToShadowMap(SDL_GPURenderPass* pass, const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData) {
        if (!m_ShadowMapSkinnedPipeline) return;
        
        SDL_BindGPUGraphicsPipeline(pass, m_ShadowMapSkinnedPipeline);
        
        m_Context.World->query<WorldTransform, MeshComponent, AnimatorComponent>()
            .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp, AnimatorComponent& anim) {
                if (!meshComp.mesh || !anim.skeleton) return;
                
                // Apply render offset to model matrix
                glm::mat4 model = t.matrix;
                if (meshComp.renderOffset != glm::vec3(0.0f)) {
                    model = glm::translate(model, meshComp.renderOffset);
                }
                
                // Push light space matrix (binding 0)
                SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &m_LightSpaceMatrix, sizeof(m_LightSpaceMatrix));
                
                // Push model matrix (binding 1)
                SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 1, &model, sizeof(model));
                
                // Push skin matrices (binding 2)
                std::vector<glm::mat4> skinMatrices(256, glm::mat4(1.0f));
                auto it = skinData.find(e.id());
                if (it != skinData.end()) {
                    skinMatrices = it->second;
                }
                SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 2, skinMatrices.data(), 256 * sizeof(glm::mat4));
                
                // Bind vertex buffer
                SDL_GPUBufferBinding vertexBinding = {};
                vertexBinding.buffer = meshComp.mesh->GetVertexBuffer();
                vertexBinding.offset = 0;
                SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
                
                // Bind index buffer
                SDL_GPUBufferBinding indexBinding = {};
                indexBinding.buffer = meshComp.mesh->GetIndexBuffer();
                indexBinding.offset = 0;
                SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                
                // Draw
                SDL_DrawGPUIndexedPrimitives(pass, meshComp.mesh->GetIndexCount(), 1, 0, 0, 0);
                m_Stats.drawCalls++;
            });
    }

    void RenderSystem::DispatchLightCulling(const glm::mat4& view, const glm::mat4& proj) {
        if (!m_LightCullingPipeline || !m_RenderDevice.IsForwardPlusEnabled()) return;
        
        m_RenderDevice.DispatchLightCulling(m_LightCullingPipeline, view, proj);
    }

    void RenderSystem::UpdateLightBufferForForwardPlus() {
        // Structure matching the shader's LightBuffer layout
        struct GPUPointLight {
            glm::vec4 positionRadius;  // xyz = position, w = radius
            glm::vec4 colorIntensity;  // rgb = color, w = intensity
            uint64_t entityId;         // For sorting to ensure consistent order
        };
        
        // Header: numLights + 3 padding ints = 16 bytes
        struct LightBufferHeader {
            int32_t numLights;
            int32_t _pad0;
            int32_t _pad1;
            int32_t _pad2;
        };
        
        // GPU-side light structure (without entityId)
        struct GPUPointLightPacked {
            glm::vec4 positionRadius;
            glm::vec4 colorIntensity;
        };
        
        constexpr int MAX_POINT_LIGHTS = 1024;
        
        // Collect all point lights with entity IDs for stable sorting
        std::vector<GPUPointLight> lights;
        lights.reserve(MAX_POINT_LIGHTS);
        
        m_Context.World->query<const WorldTransform, const PointLight>()
            .each([&](flecs::entity e, const WorldTransform& t, const PointLight& light) {
                if (lights.size() < MAX_POINT_LIGHTS) {
                    glm::vec3 pos = glm::vec3(t.matrix[3]);
                    GPUPointLight gpuLight;
                    gpuLight.positionRadius = glm::vec4(pos, light.radius);
                    gpuLight.colorIntensity = glm::vec4(light.color, light.intensity);
                    gpuLight.entityId = e.id();
                    lights.push_back(gpuLight);
                }
            });
        
        // Sort by entity ID for consistent ordering between frames
        std::sort(lights.begin(), lights.end(), [](const GPUPointLight& a, const GPUPointLight& b) {
            return a.entityId < b.entityId;
        });
        
        // Build buffer data: header + packed lights (without entityId)
        std::vector<uint8_t> bufferData;
        bufferData.resize(sizeof(LightBufferHeader) + lights.size() * sizeof(GPUPointLightPacked));
        
        LightBufferHeader header;
        header.numLights = static_cast<int32_t>(lights.size());
        header._pad0 = 0;
        header._pad1 = 0;
        header._pad2 = 0;
        
        memcpy(bufferData.data(), &header, sizeof(header));
        
        // Debug: log light positions once
        static bool loggedLights = false;
        if (!loggedLights && !lights.empty()) {
            LOG_CORE_INFO("Forward+ Light Buffer: {} lights", lights.size());
            for (size_t i = 0; i < std::min(lights.size(), size_t(4)); i++) {
                LOG_CORE_INFO("  Light[{}]: pos=({:.1f},{:.1f},{:.1f}), radius={:.1f}, intensity={:.1f}",
                    i, lights[i].positionRadius.x, lights[i].positionRadius.y, lights[i].positionRadius.z,
                    lights[i].positionRadius.w, lights[i].colorIntensity.w);
            }
            loggedLights = true;
        }
        
        // Pack lights without entityId
        for (size_t i = 0; i < lights.size(); i++) {
            GPUPointLightPacked packed;
            packed.positionRadius = lights[i].positionRadius;
            packed.colorIntensity = lights[i].colorIntensity;
            memcpy(bufferData.data() + sizeof(header) + i * sizeof(GPUPointLightPacked), 
                   &packed, sizeof(GPUPointLightPacked));
        }
        
        // Ensure Forward+ buffers exist before updating
        m_RenderDevice.EnsureForwardPlusBuffers();
        
        m_RenderDevice.UpdateLightBuffer(bufferData.data(), static_cast<uint32_t>(lights.size()));
    }

    void RenderSystem::BeginFrame(bool drawSkeleton) {
        // Cleanup resources from previous frames
        for (auto b : m_BuffersToDelete) SDL_ReleaseGPUBuffer(m_RenderDevice.GetDevice(), b);
        m_BuffersToDelete.clear();
        for (auto b : m_TransferBuffersToDelete) SDL_ReleaseGPUTransferBuffer(m_RenderDevice.GetDevice(), b);
        m_TransferBuffersToDelete.clear();

        // Generate Debug Lines BEFORE BeginFrame (which starts the RenderPass)
        // This allows us to upload data using CopyPass if needed, or just prepare data.
        // Actually, we need a command buffer to do CopyPass.
        // RenderDevice::BeginFrame acquires the command buffer.
        // So we should probably do this:
        // 1. Acquire Command Buffer (RenderDevice::BeginFrame)
        // 2. Do Uploads (CopyPass)
        // 3. Start Render Pass (RenderDevice::BeginRenderPass)
        
        // Currently RenderDevice::BeginFrame does NOT start the render pass.
        // It acquires the command buffer and swapchain texture.
        // RenderDevice::BeginRenderPass starts the pass.
        
        m_RenderDevice.BeginFrame();
        
        // Now we have a command buffer but no render pass yet.
        // We can generate lines and upload them here!
        
        m_LineVertices.clear();
        
        // Debug Draw Skeletons (conditional)
        if (drawSkeleton) {
            m_Context.World->query<WorldTransform, AnimatorComponent, MeshComponent>()
                .each([&](flecs::entity e, WorldTransform& t, AnimatorComponent& anim, MeshComponent& meshComp) {
                    if (anim.skeleton && !anim.models.empty()) {
                        const auto& parents = anim.skeleton->skeleton.joint_parents();
                        int numJoints = anim.skeleton->skeleton.num_joints();
                        
                        // Apply render offset for skeleton debug drawing (same as mesh)
                        glm::mat4 offsetMatrix = t.matrix;
                        if (meshComp.renderOffset != glm::vec3(0.0f)) {
                            offsetMatrix = glm::translate(offsetMatrix, meshComp.renderOffset);
                        }
                        
                        for (int i = 0; i < numJoints; ++i) {
                            int parent = parents[i];
                            if (parent != ozz::animation::Skeleton::kNoParent) {
                                glm::mat4 childModel;
                                memcpy(&childModel, &anim.models[i], sizeof(glm::mat4));
                                
                                glm::mat4 parentModel;
                                memcpy(&parentModel, &anim.models[parent], sizeof(glm::mat4));
                                
                                glm::vec3 p1 = glm::vec3(offsetMatrix * childModel * glm::vec4(0,0,0,1));
                                glm::vec3 p2 = glm::vec3(offsetMatrix * parentModel * glm::vec4(0,0,0,1));
                                
                                DrawLine(p1, p2, {1.0f, 1.0f, 0.0f});
                            }
                        }
                    }
                });
        }

        // Upload Lines if any
        if (!m_LineVertices.empty()) {
            SDL_GPUBufferCreateInfo bufferInfo = {};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bufferInfo.size = m_LineVertices.size() * sizeof(LineVertex);
            
            SDL_GPUBuffer* lineBuffer = SDL_CreateGPUBuffer(m_RenderDevice.GetDevice(), &bufferInfo);
            
            SDL_GPUTransferBufferCreateInfo transferInfo = {};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = bufferInfo.size;
            
            SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_RenderDevice.GetDevice(), &transferInfo);
            
            Uint8* map = (Uint8*)SDL_MapGPUTransferBuffer(m_RenderDevice.GetDevice(), transferBuffer, false);
            if (map) {
                memcpy(map, m_LineVertices.data(), bufferInfo.size);
                SDL_UnmapGPUTransferBuffer(m_RenderDevice.GetDevice(), transferBuffer);
                
                SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(m_RenderDevice.GetCommandBuffer());
                SDL_GPUTransferBufferLocation source = {};
                source.transfer_buffer = transferBuffer;
                source.offset = 0;
                
                SDL_GPUBufferRegion destination = {};
                destination.buffer = lineBuffer;
                destination.offset = 0;
                destination.size = bufferInfo.size;
                
                SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
                SDL_EndGPUCopyPass(copyPass);
            }
            
            m_CurrentLineBuffer = lineBuffer;
            m_BuffersToDelete.push_back(lineBuffer);
            m_TransferBuffersToDelete.push_back(transferBuffer);
        } else {
            m_CurrentLineBuffer = nullptr;
        }
    }

    void RenderSystem::DrawScene(double alpha) {
        (void)alpha;
        
        if (!m_Pipeline) return;

        // Build batches first (collects all mesh instances)
        BuildBatches();

        // Prepare Skinning Data (UBO-based) for animated meshes
        std::unordered_map<uint64_t, std::vector<glm::mat4>> skinData;
        
        m_Context.World->query<WorldTransform, MeshComponent, AnimatorComponent>()
            .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp, AnimatorComponent& anim) {
                if (meshComp.mesh && !anim.models.empty()) {
                    const auto& compactIBMs = meshComp.mesh->GetInverseBindMatrices();
                    const auto& jointRemaps = meshComp.mesh->GetJointRemaps();
                    size_t usedJointCount = jointRemaps.size();
                    
                    std::vector<glm::mat4> jointMatrices(256, glm::mat4(1.0f));
                    
                    for (size_t compactIdx = 0; compactIdx < usedJointCount && compactIdx < 256; ++compactIdx) {
                        uint16_t skelIdx = jointRemaps[compactIdx];
                        
                        if (skelIdx < anim.models.size()) {
                            glm::mat4 modelTransform;
                            memcpy(&modelTransform, &anim.models[skelIdx], sizeof(glm::mat4));
                            jointMatrices[compactIdx] = modelTransform * compactIBMs[compactIdx];
                        }
                    }
                    
                    skinData[e.id()] = std::move(jointMatrices);
                }
            });

        // Copy Pass - upload lines and instance buffers
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(m_RenderDevice.GetCommandBuffer());

        // Upload Lines
        SDL_GPUBuffer* lineBuffer = nullptr;
        if (!m_LineVertices.empty()) {
            SDL_GPUBufferCreateInfo bufferInfo = {};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
            bufferInfo.size = m_LineVertices.size() * sizeof(LineVertex);
            
            lineBuffer = SDL_CreateGPUBuffer(m_RenderDevice.GetDevice(), &bufferInfo);
            
            SDL_GPUTransferBufferCreateInfo transferInfo = {};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = bufferInfo.size;
            
            SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_RenderDevice.GetDevice(), &transferInfo);
            
            Uint8* map = (Uint8*)SDL_MapGPUTransferBuffer(m_RenderDevice.GetDevice(), transferBuffer, false);
            memcpy(map, m_LineVertices.data(), bufferInfo.size);
            SDL_UnmapGPUTransferBuffer(m_RenderDevice.GetDevice(), transferBuffer);
            
            SDL_GPUTransferBufferLocation source = {};
            source.transfer_buffer = transferBuffer;
            source.offset = 0;
            
            SDL_GPUBufferRegion destination = {};
            destination.buffer = lineBuffer;
            destination.offset = 0;
            destination.size = bufferInfo.size;
            
            SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
            
            m_BuffersToDelete.push_back(lineBuffer);
            m_TransferBuffersToDelete.push_back(transferBuffer);
        }
        m_CurrentLineBuffer = lineBuffer;

        // Upload Instance Buffer for all batched static meshes (shared buffer)
        // First, calculate total instance count and assign offsets
        uint32_t totalInstances = 0;
        for (auto& [meshPtr, batch] : m_Batches) {
            if (batch.instances.empty()) continue;
            batch.instanceOffset = totalInstances;
            totalInstances += static_cast<uint32_t>(batch.instances.size());
        }
        
        if (totalInstances > 0) {
            size_t requiredSize = totalInstances * sizeof(MeshInstance);
            
            // Resize persistent instance buffer if needed
            if (requiredSize > m_InstanceBufferCapacity) {
                if (m_InstanceBuffer) {
                    m_BuffersToDelete.push_back(m_InstanceBuffer);
                }
                
                // Grow by 50% extra to avoid frequent reallocations
                size_t newCapacity = static_cast<size_t>(requiredSize * 1.5);
                
                SDL_GPUBufferCreateInfo bufferInfo = {};
                bufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
                bufferInfo.size = newCapacity;
                
                m_InstanceBuffer = SDL_CreateGPUBuffer(m_RenderDevice.GetDevice(), &bufferInfo);
                m_InstanceBufferCapacity = static_cast<uint32_t>(newCapacity);
                
                LOG_CORE_INFO("Resized instance buffer to {} instances ({} bytes)", 
                              newCapacity / sizeof(MeshInstance), newCapacity);
            }
            
            if (m_InstanceBuffer) {
                // Create transfer buffer and pack all instances
                SDL_GPUTransferBufferCreateInfo transferInfo = {};
                transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
                transferInfo.size = requiredSize;
                
                SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_RenderDevice.GetDevice(), &transferInfo);
                if (transferBuffer) {
                    Uint8* map = (Uint8*)SDL_MapGPUTransferBuffer(m_RenderDevice.GetDevice(), transferBuffer, false);
                    
                    // Pack all batch instances into contiguous memory
                    size_t offset = 0;
                    for (auto& [meshPtr, batch] : m_Batches) {
                        if (batch.instances.empty()) continue;
                        size_t batchSize = batch.instances.size() * sizeof(MeshInstance);
                        memcpy(map + offset, batch.instances.data(), batchSize);
                        offset += batchSize;
                    }
                    
                    SDL_UnmapGPUTransferBuffer(m_RenderDevice.GetDevice(), transferBuffer);
                    
                    SDL_GPUTransferBufferLocation source = {};
                    source.transfer_buffer = transferBuffer;
                    source.offset = 0;
                    
                    SDL_GPUBufferRegion destination = {};
                    destination.buffer = m_InstanceBuffer;
                    destination.offset = 0;
                    destination.size = requiredSize;
                    
                    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
                    
                    m_TransferBuffersToDelete.push_back(transferBuffer);
                }
            }
        }

        SDL_EndGPUCopyPass(copyPass);

        // Get camera matrices for all passes
        glm::mat4 view = glm::mat4(1.0f);
        glm::mat4 proj = glm::mat4(1.0f);
        glm::vec3 cameraPosition = glm::vec3(0, 2, 5);
        float aspectRatio = m_RenderDevice.GetWindow()->GetAspectRatio();
        
        bool cameraFound = false;
        m_Context.World->query<LocalTransform, const CameraComponent>()
            .each([&](flecs::entity e, LocalTransform& t, const CameraComponent& cam) {
                if (cam.isPrimary && !cameraFound) {
                    cameraPosition = t.position;
                    glm::mat4 camMatrix = glm::translate(glm::mat4(1.0f), t.position);
                    camMatrix = glm::rotate(camMatrix, glm::radians(t.rotation.y), glm::vec3(0, 1, 0));
                    camMatrix = glm::rotate(camMatrix, glm::radians(t.rotation.x), glm::vec3(1, 0, 0));
                    camMatrix = glm::rotate(camMatrix, glm::radians(t.rotation.z), glm::vec3(0, 0, 1));
                    view = glm::inverse(camMatrix);
                    proj = glm::perspectiveRH_ZO(glm::radians(cam.fov), aspectRatio, cam.nearPlane, cam.farPlane);
                    cameraFound = true;
                }
            });
        
        if (!cameraFound) {
            view = glm::lookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
            proj = glm::perspectiveRH_ZO(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        }
        
        // Cache matrices for post-processing passes (SSGI, etc.)
        m_CurrentView = view;
        m_CurrentProj = proj;

        // Forward+ passes (depth pre-pass + light culling)
        if (m_RenderDevice.IsForwardPlusEnabled()) {
            // Update light buffer for light culling
            UpdateLightBufferForForwardPlus();
            
            // 2a. Depth Pre-Pass
            RenderDepthPrePass(view, proj);
            
            // 2b. Light Culling Compute Pass
            DispatchLightCulling(view, proj);
        }
        
        // Shadow Pass (render scene from light's perspective)
        if (m_RenderDevice.IsShadowsEnabled()) {
            RenderShadowPass(skinData);
        }

        // 3. Begin Main Render Pass
        if (!m_RenderDevice.BeginRenderPass()) return;
        SDL_GPURenderPass* pass = m_RenderDevice.GetRenderPass();

        if (pass) {
            // Draw Sprites
            SDL_BindGPUGraphicsPipeline(pass, m_Pipeline);
            
            m_Context.World->query<WorldTransform, SpriteComponent>()
                .each([&](flecs::entity e, WorldTransform& t, SpriteComponent& s) {
                if (s.texture) {
                    glm::mat4 model = t.matrix;
                    SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &model, sizeof(model));

                    SDL_GPUTextureSamplerBinding binding;
                    binding.texture = s.texture->GetGPUTexture();
                    binding.sampler = m_Sampler;
                    
                    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
                    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);
                }
            });

            // Draw Meshes
            if (m_MeshPipeline) {
                SDL_BindGPUGraphicsPipeline(pass, m_MeshPipeline);

                // Camera matrices (view/proj) already calculated before render pass

                // Gather lights
                constexpr int MAX_POINT_LIGHTS = 8;
                
                struct LightUBO {
                    glm::vec4 dirLightDir;      // xyz = direction, w = intensity
                    glm::vec4 dirLightColor;    // rgb = color, a = unused
                    glm::vec4 ambientColor;     // rgb = ambient, a = unused
                    glm::vec4 cameraPos;        // xyz = position, w = unused
                    glm::vec4 pointLightPos[MAX_POINT_LIGHTS];   // xyz = position, w = radius
                    glm::vec4 pointLightColor[MAX_POINT_LIGHTS]; // rgb = color, a = intensity
                    int numPointLights;
                    float shininess;
                    glm::vec2 _padding;
                    // Shadow parameters (must match Mesh.frag LightUniforms)
                    glm::mat4 lightSpaceMatrix;
                    float shadowBias;
                    float shadowNormalBias;
                    int32_t pcfSamples;
                    int32_t shadowsEnabled;
                } lightUbo;
                
                // Default directional light
                lightUbo.dirLightDir = glm::vec4(-0.5f, -1.0f, -0.3f, 1.0f);
                lightUbo.dirLightColor = glm::vec4(1.0f, 0.95f, 0.9f, 1.0f);
                lightUbo.ambientColor = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f);
                lightUbo.cameraPos = glm::vec4(cameraPosition, 1.0f);
                lightUbo.numPointLights = 0;
                lightUbo.shininess = 32.0f;
                
                // Query directional lights (use first one found)
                m_Context.World->query<const DirectionalLight>()
                    .each([&](flecs::entity e, const DirectionalLight& light) {
                        lightUbo.dirLightDir = glm::vec4(light.direction, light.intensity);
                        lightUbo.dirLightColor = glm::vec4(light.color, 1.0f);
                        lightUbo.ambientColor = glm::vec4(light.ambient, 1.0f);
                    });
                
                // Query point lights - collect all and sort by distance to camera
                struct LightInfo {
                    glm::vec3 pos;
                    float radius;
                    glm::vec3 color;
                    float intensity;
                    float distSq;
                };
                std::vector<LightInfo> allLights;
                m_Context.World->query<const WorldTransform, const PointLight>()
                    .each([&](flecs::entity e, const WorldTransform& t, const PointLight& light) {
                        glm::vec3 pos = glm::vec3(t.matrix[3]);
                        float distSq = glm::dot(pos - cameraPosition, pos - cameraPosition);
                        allLights.push_back({pos, light.radius, light.color, light.intensity, distSq});
                    });
                
                // Sort by distance (closest first)
                std::sort(allLights.begin(), allLights.end(), 
                    [](const LightInfo& a, const LightInfo& b) { return a.distSq < b.distSq; });
                
                // Take up to MAX_POINT_LIGHTS closest
                int pointLightIdx = 0;
                for (const auto& light : allLights) {
                    if (pointLightIdx >= MAX_POINT_LIGHTS) break;
                    lightUbo.pointLightPos[pointLightIdx] = glm::vec4(light.pos, light.radius);
                    lightUbo.pointLightColor[pointLightIdx] = glm::vec4(light.color, light.intensity);
                    pointLightIdx++;
                }
                lightUbo.numPointLights = pointLightIdx;
                
                // Shadow parameters
                lightUbo.lightSpaceMatrix = m_LightSpaceMatrix;
                lightUbo.shadowBias = m_RenderDevice.GetShadowBias();
                lightUbo.shadowNormalBias = m_RenderDevice.GetShadowNormalBias();
                lightUbo.pcfSamples = m_RenderDevice.GetShadowPcfSamples();
                lightUbo.shadowsEnabled = m_RenderDevice.IsShadowsEnabled() ? 1 : 0;

                // Render batched static meshes
                if (m_RenderDevice.IsForwardPlusEnabled() && m_ForwardPlusPipeline) {
                    // Use Forward+ path with tile-based light culling
                    static bool loggedPath = false;
                    if (!loggedPath) { LOG_CORE_INFO("Using Forward+ rendering path"); loggedPath = true; }
                    RenderBatchesForwardPlus(pass, view, proj);
                } else {
                    // Traditional forward path (limited to 8 point lights)
                    static bool loggedPath2 = false;
                    if (!loggedPath2) { LOG_CORE_INFO("Using Traditional rendering path"); loggedPath2 = true; }
                    RenderBatches(pass, view, proj, &lightUbo, sizeof(lightUbo));
                }
                
                // Render skinned/animated meshes (still uses regular pipeline for now)
                RenderSkinnedMeshes(pass, view, proj, &lightUbo, sizeof(lightUbo), skinData);

                // Render Lines
                if (m_LinePipeline && m_CurrentLineBuffer && !m_LineVertices.empty()) {
                    SDL_BindGPUGraphicsPipeline(pass, m_LinePipeline);
                    
                    struct UBO {
                        glm::mat4 view;
                        glm::mat4 proj;
                    } ubo;
                    
                    ubo.view = view;
                    ubo.proj = proj;
                    
                    SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &ubo, sizeof(ubo));
                    
                    SDL_GPUBufferBinding binding = {};
                    binding.buffer = m_CurrentLineBuffer;
                    binding.offset = 0;
                    
                    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
                    SDL_DrawGPUPrimitives(pass, m_LineVertices.size(), 1, 0, 0);
                }
            }
            
            m_LineVertices.clear();
        }
    }

    void RenderSystem::DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) {
        m_LineVertices.push_back({start, color});
        m_LineVertices.push_back({end, color});
    }

    void RenderSystem::DrawWireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, const glm::quat& rotation) {
        // 8 corners of the box
        glm::vec3 corners[8] = {
            {-halfExtents.x, -halfExtents.y, -halfExtents.z},
            { halfExtents.x, -halfExtents.y, -halfExtents.z},
            { halfExtents.x,  halfExtents.y, -halfExtents.z},
            {-halfExtents.x,  halfExtents.y, -halfExtents.z},
            {-halfExtents.x, -halfExtents.y,  halfExtents.z},
            { halfExtents.x, -halfExtents.y,  halfExtents.z},
            { halfExtents.x,  halfExtents.y,  halfExtents.z},
            {-halfExtents.x,  halfExtents.y,  halfExtents.z},
        };
        
        // Apply rotation and translation
        for (int i = 0; i < 8; i++) {
            corners[i] = center + rotation * corners[i];
        }
        
        // Draw 12 edges
        // Bottom face
        DrawLine(corners[0], corners[1], color);
        DrawLine(corners[1], corners[2], color);
        DrawLine(corners[2], corners[3], color);
        DrawLine(corners[3], corners[0], color);
        // Top face
        DrawLine(corners[4], corners[5], color);
        DrawLine(corners[5], corners[6], color);
        DrawLine(corners[6], corners[7], color);
        DrawLine(corners[7], corners[4], color);
        // Vertical edges
        DrawLine(corners[0], corners[4], color);
        DrawLine(corners[1], corners[5], color);
        DrawLine(corners[2], corners[6], color);
        DrawLine(corners[3], corners[7], color);
    }

    void RenderSystem::DrawWireSphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments) {
        const float PI = 3.14159265359f;
        
        // Draw 3 circles (XY, XZ, YZ planes)
        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)(i + 1) / segments * 2.0f * PI;
            
            // XY circle
            glm::vec3 p1 = center + glm::vec3(cos(angle1) * radius, sin(angle1) * radius, 0);
            glm::vec3 p2 = center + glm::vec3(cos(angle2) * radius, sin(angle2) * radius, 0);
            DrawLine(p1, p2, color);
            
            // XZ circle
            p1 = center + glm::vec3(cos(angle1) * radius, 0, sin(angle1) * radius);
            p2 = center + glm::vec3(cos(angle2) * radius, 0, sin(angle2) * radius);
            DrawLine(p1, p2, color);
            
            // YZ circle
            p1 = center + glm::vec3(0, cos(angle1) * radius, sin(angle1) * radius);
            p2 = center + glm::vec3(0, cos(angle2) * radius, sin(angle2) * radius);
            DrawLine(p1, p2, color);
        }
    }

    void RenderSystem::DrawWireCapsule(const glm::vec3& center, float halfHeight, float radius, const glm::vec3& color, int segments) {
        const float PI = 3.14159265359f;
        
        // Top and bottom sphere centers
        glm::vec3 topCenter = center + glm::vec3(0, halfHeight, 0);
        glm::vec3 bottomCenter = center - glm::vec3(0, halfHeight, 0);
        
        // Draw vertical lines connecting spheres
        for (int i = 0; i < 4; i++) {
            float angle = (float)i / 4.0f * 2.0f * PI;
            glm::vec3 offset(cos(angle) * radius, 0, sin(angle) * radius);
            DrawLine(topCenter + offset, bottomCenter + offset, color);
        }
        
        // Draw top hemisphere
        for (int i = 0; i < segments; i++) {
            float angle1 = (float)i / segments * 2.0f * PI;
            float angle2 = (float)(i + 1) / segments * 2.0f * PI;
            
            // Horizontal circle at top
            glm::vec3 p1 = topCenter + glm::vec3(cos(angle1) * radius, 0, sin(angle1) * radius);
            glm::vec3 p2 = topCenter + glm::vec3(cos(angle2) * radius, 0, sin(angle2) * radius);
            DrawLine(p1, p2, color);
            
            // Horizontal circle at bottom
            p1 = bottomCenter + glm::vec3(cos(angle1) * radius, 0, sin(angle1) * radius);
            p2 = bottomCenter + glm::vec3(cos(angle2) * radius, 0, sin(angle2) * radius);
            DrawLine(p1, p2, color);
        }
        
        // Draw hemisphere arcs (XY and ZY planes)
        for (int i = 0; i < segments / 2; i++) {
            float angle1 = (float)i / segments * PI;
            float angle2 = (float)(i + 1) / segments * PI;
            
            // Top hemisphere - XY arc
            glm::vec3 p1 = topCenter + glm::vec3(sin(angle1) * radius, cos(angle1) * radius, 0);
            glm::vec3 p2 = topCenter + glm::vec3(sin(angle2) * radius, cos(angle2) * radius, 0);
            DrawLine(p1, p2, color);
            
            // Top hemisphere - ZY arc
            p1 = topCenter + glm::vec3(0, cos(angle1) * radius, sin(angle1) * radius);
            p2 = topCenter + glm::vec3(0, cos(angle2) * radius, sin(angle2) * radius);
            DrawLine(p1, p2, color);
            
            // Bottom hemisphere - XY arc
            p1 = bottomCenter + glm::vec3(sin(angle1) * radius, -cos(angle1) * radius, 0);
            p2 = bottomCenter + glm::vec3(sin(angle2) * radius, -cos(angle2) * radius, 0);
            DrawLine(p1, p2, color);
            
            // Bottom hemisphere - ZY arc
            p1 = bottomCenter + glm::vec3(0, -cos(angle1) * radius, sin(angle1) * radius);
            p2 = bottomCenter + glm::vec3(0, -cos(angle2) * radius, sin(angle2) * radius);
            DrawLine(p1, p2, color);
        }
    }

    void RenderSystem::DrawPhysicsDebug() {
        // Draw colliders for all entities with Collider component
        m_Context.World->query<const LocalTransform, const Collider>()
            .each([this](flecs::entity e, const LocalTransform& transform, const Collider& collider) {
                glm::vec3 color = {0.0f, 1.0f, 0.0f}; // Green for static
                
                // Check if it has a RigidBody to determine color
                if (e.has<RigidBody>()) {
                    const RigidBody& rb = e.get<RigidBody>();
                    if (rb.motionType == MotionType::Dynamic) {
                        color = {1.0f, 0.5f, 0.0f}; // Orange for dynamic
                    } else if (rb.motionType == MotionType::Kinematic) {
                        color = {0.0f, 0.5f, 1.0f}; // Blue for kinematic
                    }
                }
                
                glm::vec3 center = transform.position + collider.offset;
                glm::quat rotation = glm::quat(glm::radians(transform.rotation));
                
                switch (collider.type) {
                    case ColliderType::Box:
                        DrawWireBox(center, collider.size, color, rotation);
                        break;
                    case ColliderType::Sphere:
                        DrawWireSphere(center, collider.size.x, color);
                        break;
                    case ColliderType::Capsule:
                        DrawWireCapsule(center, collider.size.y * 0.5f, collider.size.x, color);
                        break;
                    default:
                        break;
                }
            });
        
        // Draw character physics capsules
        m_Context.World->query<const LocalTransform, const CharacterPhysics>()
            .each([this](flecs::entity e, const LocalTransform& transform, const CharacterPhysics& physics) {
                glm::vec3 color = {1.0f, 1.0f, 0.0f}; // Yellow for character
                if (physics.isOnGround) {
                    color = {0.0f, 1.0f, 1.0f}; // Cyan when grounded
                }
                
                // Character capsule center is at transform position
                DrawWireCapsule(transform.position, physics.height * 0.5f, physics.radius, color);
            });
    }

    void RenderSystem::BuildBatches() {
        m_Batches.clear();
        m_Stats.Reset();
        
        m_Context.World->query<WorldTransform, MeshComponent>()
            .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp) {
                if (!meshComp.mesh) return;
                
                m_Stats.totalInstances++;
                
                // Check if entity has animation/skinning - skip for batching
                bool hasSkinning = e.has<AnimatorComponent>() && e.get<AnimatorComponent>().skeleton;
                
                if (hasSkinning) {
                    m_Stats.skinnedInstances++;
                    return;  // Skinned meshes are rendered separately, not batched
                }
                
                m_Stats.batchedInstances++;
                
                Resources::Mesh* meshPtr = meshComp.mesh.get();
                
                // Get or create batch for this mesh
                auto& batch = m_Batches[meshPtr];
                batch.mesh = meshComp.mesh;
                
                // Build model matrix with render offset
                glm::mat4 model = t.matrix;
                if (meshComp.renderOffset != glm::vec3(0.0f)) {
                    model = glm::translate(model, meshComp.renderOffset);
                }
                
                MeshInstance instance;
                instance.model = model;
                instance.color = glm::vec4(1.0f);  // Default white, could use material color
                
                batch.instances.push_back(instance);
            });
    }

    void RenderSystem::RenderBatches(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj, 
                                      const void* lightUbo, size_t lightUboSize) {
        if (!m_InstancedMeshPipeline || !m_InstanceBuffer) {
            return;
        }
        
        // Check if we have any batches to render
        bool hasBatches = false;
        for (auto& [meshPtr, batch] : m_Batches) {
            if (!batch.instances.empty()) {
                hasBatches = true;
                break;
            }
        }
        
        if (!hasBatches) return;
        
        SDL_BindGPUGraphicsPipeline(pass, m_InstancedMeshPipeline);
        
        // Push ViewProj uniform
        struct ViewProjUBO {
            glm::mat4 view;
            glm::mat4 proj;
        } viewProjUbo;
        viewProjUbo.view = view;
        viewProjUbo.proj = proj;
        
        for (auto& [meshPtr, batch] : m_Batches) {
            if (batch.instances.empty()) continue;
            
            // Push uniforms per batch
            SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &viewProjUbo, sizeof(viewProjUbo));
            SDL_PushGPUFragmentUniformData(m_RenderDevice.GetCommandBuffer(), 0, lightUbo, lightUboSize);
            
            // Bind vertex buffers (mesh vertices + instance data with offset)
            SDL_GPUBufferBinding bindings[2];
            bindings[0].buffer = batch.mesh->GetVertexBuffer();
            bindings[0].offset = 0;
            bindings[1].buffer = m_InstanceBuffer;
            bindings[1].offset = batch.instanceOffset * sizeof(MeshInstance);
            
            SDL_BindGPUVertexBuffers(pass, 0, bindings, 2);
            
            // Bind index buffer
            SDL_GPUBufferBinding indexBinding;
            indexBinding.buffer = batch.mesh->GetIndexBuffer();
            indexBinding.offset = 0;
            SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            
            // Draw instanced
            Uint32 indexCount = batch.mesh->GetIndexCount();
            Uint32 instanceCount = static_cast<Uint32>(batch.instances.size());
            SDL_DrawGPUIndexedPrimitives(pass, indexCount, instanceCount, 0, 0, 0);
            
            m_Stats.drawCalls++;
        }
    }

    void RenderSystem::RenderBatchesForwardPlus(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj) {
        if (!m_ForwardPlusPipeline || !m_InstanceBuffer) {
            return;
        }
        
        // Check buffers exist
        SDL_GPUBuffer* lightBuffer = m_RenderDevice.GetLightBuffer();
        SDL_GPUBuffer* tileBuffer = m_RenderDevice.GetTileLightIndicesBuffer();
        if (!lightBuffer || !tileBuffer) {
            LOG_CORE_WARN("Forward+ buffers not available, skipping Forward+ rendering");
            return;
        }
        
        // Check if we have any batches to render
        bool hasBatches = false;
        for (auto& [meshPtr, batch] : m_Batches) {
            if (!batch.instances.empty()) {
                hasBatches = true;
                break;
            }
        }
        
        if (!hasBatches) return;
        
        SDL_BindGPUGraphicsPipeline(pass, m_ForwardPlusPipeline);
        
        // SDL_GPU requires samplers to be bound first, then storage buffers
        // Shader layout: binding 0 = shadow sampler, binding 1 = light buffer, binding 2 = tile indices
        
        // Bind shadow map sampler - Fragment set 2, binding 0
        SDL_GPUTexture* shadowMapTex = m_RenderDevice.GetShadowMapTexture();
        SDL_GPUSampler* shadowSampler = m_RenderDevice.GetShadowSampler();
        if (shadowMapTex && shadowSampler) {
            SDL_GPUTextureSamplerBinding shadowBinding = {};
            shadowBinding.texture = shadowMapTex;
            shadowBinding.sampler = shadowSampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &shadowBinding, 1);
        }
        
        // Bind storage buffers (light buffer and tile indices) - Fragment set 2, bindings 1 and 2
        SDL_GPUBuffer* storageBuffers[2] = { lightBuffer, tileBuffer };
        SDL_BindGPUFragmentStorageBuffers(pass, 0, storageBuffers, 2);
        
        // Prepare uniforms
        struct ViewProjUBO {
            glm::mat4 view;
            glm::mat4 proj;
        } viewProjUbo;
        viewProjUbo.view = view;
        viewProjUbo.proj = proj;
        
        // Forward+ fragment shader uniform (includes directional light + screen info + shadows)
        struct ForwardPlusFragmentUBO {
            glm::vec4 dirLightDir;      // xyz = direction, w = intensity
            glm::vec4 dirLightColor;    // rgb = color, a = unused
            glm::vec4 ambientColor;     // rgb = ambient, a = unused
            glm::vec4 cameraPos;        // xyz = position, w = unused
            glm::vec4 screenSize;       // xy = screen size, zw = 1/size
            // Shadow parameters
            glm::mat4 lightSpaceMatrix;
            float shadowBias;
            float shadowNormalBias;
            int32_t pcfSamples;
            int32_t shadowsEnabled;
            // Original padding
            float shininess;
            float _pad1;
            float _pad2;
            float _pad3;
        } fragUbo;
        static_assert(sizeof(ForwardPlusFragmentUBO) == 176, "UBO size mismatch with shader!");
        
        // Log once for debugging
        static bool loggedOnce = false;
        if (!loggedOnce) {
            LOG_CORE_INFO("ForwardPlusFragmentUBO size: {}, shadowBias offset: {}", 
                sizeof(ForwardPlusFragmentUBO), offsetof(ForwardPlusFragmentUBO, shadowBias));
            loggedOnce = true;
        }
        
        // Default values
        fragUbo.dirLightDir = glm::vec4(-0.5f, -1.0f, -0.3f, 1.0f);
        fragUbo.dirLightColor = glm::vec4(1.0f, 0.95f, 0.9f, 1.0f);
        fragUbo.ambientColor = glm::vec4(0.15f, 0.15f, 0.2f, 1.0f);
        fragUbo.cameraPos = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        float screenW = static_cast<float>(m_RenderDevice.GetRenderWidth());
        float screenH = static_cast<float>(m_RenderDevice.GetRenderHeight());
        fragUbo.screenSize = glm::vec4(screenW, screenH, 1.0f / screenW, 1.0f / screenH);
        
        // Shadow settings
        fragUbo.lightSpaceMatrix = m_LightSpaceMatrix;
        fragUbo.shadowBias = m_RenderDevice.GetShadowBias();
        fragUbo.shadowNormalBias = m_RenderDevice.GetShadowNormalBias();
        fragUbo.pcfSamples = m_RenderDevice.GetShadowPcfSamples();
        fragUbo.shadowsEnabled = m_RenderDevice.IsShadowsEnabled() ? 1 : 0;
        
        fragUbo.shininess = 32.0f;
        
        // Query directional light
        m_Context.World->query<const DirectionalLight>()
            .each([&](flecs::entity e, const DirectionalLight& light) {
                fragUbo.dirLightDir = glm::vec4(light.direction, light.intensity);
                fragUbo.dirLightColor = glm::vec4(light.color, 1.0f);
                fragUbo.ambientColor = glm::vec4(light.ambient, 1.0f);
            });
        
        // Get camera position
        m_Context.World->query<LocalTransform, const CameraComponent>()
            .each([&](flecs::entity e, LocalTransform& t, const CameraComponent& cam) {
                if (cam.isPrimary) {
                    fragUbo.cameraPos = glm::vec4(t.position, 1.0f);
                }
            });
        
        for (auto& [meshPtr, batch] : m_Batches) {
            if (batch.instances.empty()) continue;
            
            // Push uniforms per batch
            SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &viewProjUbo, sizeof(viewProjUbo));
            SDL_PushGPUFragmentUniformData(m_RenderDevice.GetCommandBuffer(), 0, &fragUbo, sizeof(fragUbo));
            
            // Bind vertex buffers (mesh vertices + instance data with offset)
            SDL_GPUBufferBinding bindings[2];
            bindings[0].buffer = batch.mesh->GetVertexBuffer();
            bindings[0].offset = 0;
            bindings[1].buffer = m_InstanceBuffer;
            bindings[1].offset = batch.instanceOffset * sizeof(MeshInstance);
            
            SDL_BindGPUVertexBuffers(pass, 0, bindings, 2);
            
            // Bind index buffer
            SDL_GPUBufferBinding indexBinding;
            indexBinding.buffer = batch.mesh->GetIndexBuffer();
            indexBinding.offset = 0;
            SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
            
            // Draw instanced
            Uint32 indexCount = batch.mesh->GetIndexCount();
            Uint32 instanceCount = static_cast<Uint32>(batch.instances.size());
            SDL_DrawGPUIndexedPrimitives(pass, indexCount, instanceCount, 0, 0, 0);
            
            m_Stats.drawCalls++;
        }
    }

    void RenderSystem::RenderSkinnedMeshes(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj,
                                            const void* lightUbo, size_t lightUboSize,
                                            const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData) {
        if (!m_MeshPipeline) return;
        
        SDL_BindGPUGraphicsPipeline(pass, m_MeshPipeline);
        
        m_Context.World->query<WorldTransform, MeshComponent, AnimatorComponent>()
            .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp, AnimatorComponent& anim) {
                if (!meshComp.mesh || !anim.skeleton) return;
                
                struct SceneUBO {
                    glm::mat4 model;
                    glm::mat4 view;
                    glm::mat4 proj;
                } sceneUbo;
                
                // Apply render offset
                glm::mat4 model = t.matrix;
                if (meshComp.renderOffset != glm::vec3(0.0f)) {
                    model = glm::translate(model, meshComp.renderOffset);
                }
                sceneUbo.model = model;
                sceneUbo.view = view;
                sceneUbo.proj = proj;

                SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &sceneUbo, sizeof(sceneUbo));

                // Push Skin UBO (256 matrices)
                std::vector<glm::mat4> skinMatrices(256, glm::mat4(1.0f));
                auto it = skinData.find(e.id());
                if (it != skinData.end()) {
                    skinMatrices = it->second;
                }
                SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 1, skinMatrices.data(), 256 * sizeof(glm::mat4));
                
                // Push Light UBO
                SDL_PushGPUFragmentUniformData(m_RenderDevice.GetCommandBuffer(), 0, lightUbo, lightUboSize);
                
                // Bind shadow map sampler (set 2, binding 0)
                SDL_GPUTextureSamplerBinding shadowBinding;
                shadowBinding.texture = m_RenderDevice.GetShadowMapTexture();
                shadowBinding.sampler = m_RenderDevice.GetShadowSampler();
                SDL_BindGPUFragmentSamplers(pass, 0, &shadowBinding, 1);
                
                // Bind buffers
                SDL_GPUBufferBinding vertexBinding;
                vertexBinding.buffer = meshComp.mesh->GetVertexBuffer();
                vertexBinding.offset = 0;
                
                SDL_GPUBufferBinding indexBinding;
                indexBinding.buffer = meshComp.mesh->GetIndexBuffer();
                indexBinding.offset = 0;

                SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);
                SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
                
                Uint32 indexCount = meshComp.mesh->GetIndexCount();
                SDL_DrawGPUIndexedPrimitives(pass, indexCount, 1, 0, 0, 0);
                
                m_Stats.drawCalls++;
            });
    }

    void RenderSystem::RenderBloomPass() {
        // Check if bloom is enabled and we have all required resources
        if (!m_RenderDevice.IsBloomEnabled() || !m_RenderDevice.IsHDREnabled()) {
            return;
        }
        if (!m_BloomBrightPassPipeline || !m_BloomBlurPipeline) {
            return;
        }
        if (!m_RenderDevice.IsFrameValid()) {
            return;
        }
        
        SDL_GPUTexture* hdrTexture = m_RenderDevice.GetHDRTexture();
        SDL_GPUTexture* brightTexture = m_RenderDevice.GetBloomBrightTexture();
        SDL_GPUTexture* blurTextureA = m_RenderDevice.GetBloomBlurTextureA();
        SDL_GPUTexture* blurTextureB = m_RenderDevice.GetBloomBlurTextureB();
        
        if (!hdrTexture || !brightTexture || !blurTextureA || !blurTextureB) {
            return;
        }
        
        SDL_GPUCommandBuffer* cmdBuffer = m_RenderDevice.GetCommandBuffer();
        if (!cmdBuffer) return;
        
        // End the current render pass (scene rendering is done)
        m_RenderDevice.EndRenderPass();
        
        uint32_t bloomWidth = m_RenderDevice.GetRenderWidth() / 2;
        uint32_t bloomHeight = m_RenderDevice.GetRenderHeight() / 2;
        
        // ========== BRIGHT PASS ==========
        // Extract bright pixels from HDR scene into bright texture
        {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = brightTexture;
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            colorTarget.clear_color = {0, 0, 0, 1};
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_GPUViewport viewport = {};
            viewport.w = static_cast<float>(bloomWidth);
            viewport.h = static_cast<float>(bloomHeight);
            viewport.max_depth = 1.0f;
            SDL_SetGPUViewport(pass, &viewport);
            
            SDL_BindGPUGraphicsPipeline(pass, m_BloomBrightPassPipeline);
            
            SDL_GPUTextureSamplerBinding texBinding = {};
            texBinding.texture = hdrTexture;
            texBinding.sampler = m_LinearSampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &texBinding, 1);
            
            struct BrightPassParams {
                float threshold;
                float softThreshold;
                float _padding1;
                float _padding2;
            } params;
            params.threshold = m_RenderDevice.GetBloomThreshold();
            params.softThreshold = 0.5f;  // Soft knee for smoother transition
            params._padding1 = 0.0f;
            params._padding2 = 0.0f;
            
            // Debug: Log threshold changes
            static float lastThreshold = -1.0f;
            if (std::abs(params.threshold - lastThreshold) > 0.01f) {
                LOG_CORE_INFO("Bloom threshold changed: {}", params.threshold);
                lastThreshold = params.threshold;
            }
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &params, sizeof(params));
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            
            SDL_EndGPURenderPass(pass);
            m_Stats.drawCalls++;
        }
        
        // ========== BLUR PASSES (Ping-Pong) ==========
        float texelWidth = 1.0f / static_cast<float>(bloomWidth);
        float texelHeight = 1.0f / static_cast<float>(bloomHeight);
        
        int blurPasses = m_RenderDevice.GetBloomBlurPasses();
        SDL_GPUTexture* readTexture = brightTexture;
        SDL_GPUTexture* writeTexture = blurTextureA;
        
        for (int i = 0; i < blurPasses * 2; i++) {  // *2 for horizontal + vertical
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = writeTexture;
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            colorTarget.clear_color = {0, 0, 0, 1};
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_GPUViewport viewport = {};
            viewport.w = static_cast<float>(bloomWidth);
            viewport.h = static_cast<float>(bloomHeight);
            viewport.max_depth = 1.0f;
            SDL_SetGPUViewport(pass, &viewport);
            
            SDL_BindGPUGraphicsPipeline(pass, m_BloomBlurPipeline);
            
            SDL_GPUTextureSamplerBinding texBinding = {};
            texBinding.texture = readTexture;
            texBinding.sampler = m_LinearSampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &texBinding, 1);
            
            struct BlurParams {
                float dirX, dirY;
                float texelSizeX, texelSizeY;
            } params;
            
            // Alternate between horizontal (1,0) and vertical (0,1)
            bool horizontal = (i % 2 == 0);
            params.dirX = horizontal ? 1.0f : 0.0f;
            params.dirY = horizontal ? 0.0f : 1.0f;
            params.texelSizeX = texelWidth;
            params.texelSizeY = texelHeight;
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &params, sizeof(params));
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            
            SDL_EndGPURenderPass(pass);
            m_Stats.drawCalls++;
            
            // Swap read/write textures (ping-pong)
            if (i == 0) {
                readTexture = blurTextureA;
                writeTexture = blurTextureB;
            } else {
                std::swap(readTexture, writeTexture);
            }
        }
        
        // The final blurred bloom result is in 'writeTexture' (since we swapped after last write)
        // Actually, after the last iteration we wrote to writeTexture, then swapped.
        // So the result is in readTexture (which was the last writeTexture before swap).
        m_BloomResultTexture = readTexture;
        
        // Debug: Log that bloom was processed
        static bool loggedOnce = false;
        if (!loggedOnce) {
            LOG_CORE_INFO("Bloom pass completed. Result texture: {}", (void*)m_BloomResultTexture);
            loggedOnce = true;
        }
    }

    void RenderSystem::RenderToneMappingPass() {
        if (!m_ToneMappingPipeline || !m_RenderDevice.IsHDREnabled()) {
            return;
        }
        
        // Skip if frame is not valid (e.g., window minimized)
        if (!m_RenderDevice.IsFrameValid()) {
            return;
        }
        
        SDL_GPUTexture* hdrTexture = m_RenderDevice.GetHDRTexture();
        if (!hdrTexture) {
            return;
        }
        
        // End the current HDR render pass (if not already ended by bloom pass)
        m_RenderDevice.EndRenderPass();
        
        // Begin a new render pass targeting the swapchain
        if (!m_RenderDevice.BeginToneMappingPass()) {
            LOG_CORE_ERROR("Failed to begin tone mapping pass!");
            return;
        }
        
        SDL_GPURenderPass* pass = m_RenderDevice.GetRenderPass();
        if (!pass) {
            return;
        }
        
        // Set viewport to match swapchain dimensions
        SDL_GPUViewport viewport = {};
        viewport.x = 0;
        viewport.y = 0;
        viewport.w = static_cast<float>(m_RenderDevice.GetRenderWidth());
        viewport.h = static_cast<float>(m_RenderDevice.GetRenderHeight());
        viewport.min_depth = 0.0f;
        viewport.max_depth = 1.0f;
        SDL_SetGPUViewport(pass, &viewport);
        
        // Bind the tone mapping pipeline
        SDL_BindGPUGraphicsPipeline(pass, m_ToneMappingPipeline);
        
        // Bind HDR texture and bloom texture as samplers
        SDL_GPUTextureSamplerBinding texBindings[2] = {};
        texBindings[0].texture = hdrTexture;
        texBindings[0].sampler = m_LinearSampler;
        
        // Use bloom result if available, otherwise use HDR texture as placeholder (will be ignored with intensity 0)
        texBindings[1].texture = m_BloomResultTexture ? m_BloomResultTexture : hdrTexture;
        texBindings[1].sampler = m_LinearSampler;
        
        SDL_BindGPUFragmentSamplers(pass, 0, texBindings, 2);
        
        // Push tone mapping parameters
        struct ToneMappingParams {
            float exposure;
            float gamma;
            int32_t tonemapOperator;
            float bloomIntensity;
        } params;
        
        params.exposure = m_RenderDevice.GetExposure();
        params.gamma = m_RenderDevice.GetGamma();
        params.tonemapOperator = static_cast<int32_t>(m_RenderDevice.GetToneMapOperator());
        // Only apply bloom if we have a result texture
        params.bloomIntensity = (m_BloomResultTexture && m_RenderDevice.IsBloomEnabled()) 
                                 ? m_RenderDevice.GetBloomIntensity() : 0.0f;
        
        // Debug: Log bloom intensity changes
        static float lastIntensity = -1.0f;
        if (std::abs(params.bloomIntensity - lastIntensity) > 0.01f) {
            LOG_CORE_INFO("Bloom intensity changed: {}", params.bloomIntensity);
            lastIntensity = params.bloomIntensity;
        }
        
        SDL_PushGPUFragmentUniformData(m_RenderDevice.GetCommandBuffer(), 0, &params, sizeof(params));
        
        // Draw fullscreen triangle (3 vertices, generated in shader)
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        
        m_Stats.drawCalls++;
        
        // Reset bloom result texture for next frame
        m_BloomResultTexture = nullptr;
    }

    void RenderSystem::RenderSSGIPass(const glm::mat4& view, const glm::mat4& proj) {
        if (!m_SSGIPipeline || !m_RenderDevice.IsSSGIEnabled()) return;
        
        SDL_GPUTexture* ssgiTexture = m_RenderDevice.GetSSGITexture();
        SDL_GPUTexture* ssgiHistoryTexture = m_RenderDevice.GetSSGIHistoryTexture();
        SDL_GPUTexture* ssgiDenoiseTexture = m_RenderDevice.GetSSGIDenoiseTexture();
        SDL_GPUTexture* hdrTexture = m_RenderDevice.GetHDRTexture();
        SDL_GPUTexture* depthTexture = m_RenderDevice.GetDepthTexture();
        SDL_GPUTexture* noiseTexture = m_RenderDevice.GetNoiseTexture();
        
        if (!ssgiTexture || !ssgiHistoryTexture || !hdrTexture || !depthTexture) {
            return;
        }
        
        // Reset frame index when SSGI textures are recreated (e.g., window resize)
        // This ensures we don't blend with garbage history buffer
        if (m_RenderDevice.WasSSGIReset()) {
            m_FrameIndex = 0;
            m_RenderDevice.ClearSSGIResetFlag();
        }
        
        SDL_GPUCommandBuffer* cmdBuffer = m_RenderDevice.GetCommandBuffer();
        if (!cmdBuffer) return;
        if (!cmdBuffer) return;
        
        // End the current render pass (scene rendering is done)
        m_RenderDevice.EndRenderPass();
        
        // Calculate matrices
        glm::mat4 invView = glm::inverse(view);
        glm::mat4 invProj = glm::inverse(proj);
        glm::mat4 viewProj = proj * view;
        
        // ========== Pass 1: SSGI Ray Marching ==========
        {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = ssgiTexture;
            colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            colorTarget.clear_color = { 0.0f, 0.0f, 0.0f, 0.0f };
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_BindGPUGraphicsPipeline(pass, m_SSGIPipeline);
            
            // Bind samplers: color, depth, normal (using depth to reconstruct), noise
            SDL_GPUTextureSamplerBinding texBindings[4] = {};
            texBindings[0].texture = hdrTexture;
            texBindings[0].sampler = m_LinearSampler;
            texBindings[1].texture = depthTexture;
            texBindings[1].sampler = m_LinearSampler;
            texBindings[2].texture = depthTexture;  // Using depth for normal reconstruction
            texBindings[2].sampler = m_LinearSampler;
            texBindings[3].texture = noiseTexture ? noiseTexture : depthTexture;  // Fallback if no noise
            texBindings[3].sampler = m_Sampler;  // Nearest for noise
            
            SDL_BindGPUFragmentSamplers(pass, 0, texBindings, 4);
            
            // SSGI parameters
            struct SSGIParams {
                glm::mat4 viewMatrix;
                glm::mat4 projMatrix;
                glm::mat4 invViewMatrix;
                glm::mat4 invProjMatrix;
                glm::mat4 prevViewProjMatrix;
                glm::vec4 cameraPos;
                glm::vec4 screenSize;
                float nearPlane;
                float farPlane;
                float intensity;
                float maxDistance;
                int32_t numRays;
                int32_t numSteps;
                float thickness;
                float frameIndex;
            } params;
            
            params.viewMatrix = view;
            params.projMatrix = proj;
            params.invViewMatrix = invView;
            params.invProjMatrix = invProj;
            params.prevViewProjMatrix = m_PrevViewProjMatrix;
            params.cameraPos = glm::vec4(invView[3]);  // Extract camera position
            // Use full resolution for screen size since we sample depth at full res
            // The shader uses these for pixel coordinate calculations  
            params.screenSize = glm::vec4(
                static_cast<float>(m_RenderDevice.GetRenderWidth()),
                static_cast<float>(m_RenderDevice.GetRenderHeight()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderWidth()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderHeight())
            );
            params.nearPlane = 0.1f;
            params.farPlane = 100.0f;
            params.intensity = m_RenderDevice.GetSSGIIntensity();
            params.maxDistance = m_RenderDevice.GetSSGIMaxDistance();
            params.numRays = m_RenderDevice.GetSSGINumRays();
            params.numSteps = m_RenderDevice.GetSSGINumSteps();
            params.thickness = 0.1f;
            params.frameIndex = static_cast<float>(m_FrameIndex);
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &params, sizeof(params));
            
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            m_Stats.drawCalls++;
            
            SDL_EndGPURenderPass(pass);
        }
        
        // ========== Pass 2: Temporal Accumulation ==========
        if (m_SSGITemporalPipeline && ssgiHistoryTexture) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = ssgiDenoiseTexture;  // Output to denoise texture (swap buffers)
            colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_BindGPUGraphicsPipeline(pass, m_SSGITemporalPipeline);
            
            SDL_GPUTextureSamplerBinding texBindings[4] = {};
            texBindings[0].texture = ssgiTexture;        // Current GI
            texBindings[0].sampler = m_LinearSampler;
            texBindings[1].texture = ssgiHistoryTexture; // History
            texBindings[1].sampler = m_LinearSampler;
            texBindings[2].texture = depthTexture;       // Depth
            texBindings[2].sampler = m_LinearSampler;
            texBindings[3].texture = depthTexture;       // Velocity (placeholder)
            texBindings[3].sampler = m_LinearSampler;
            
            SDL_BindGPUFragmentSamplers(pass, 0, texBindings, 4);
            
            struct TemporalParams {
                glm::mat4 viewMatrix;
                glm::mat4 projMatrix;
                glm::mat4 invViewMatrix;
                glm::mat4 invProjMatrix;
                glm::mat4 prevViewProjMatrix;
                glm::vec4 screenSize;
                float temporalBlend;
                float depthThreshold;
                float normalThreshold;
                int32_t useVelocity;
            } tParams;
            
            tParams.viewMatrix = view;
            tParams.projMatrix = proj;
            tParams.invViewMatrix = invView;
            tParams.invProjMatrix = invProj;
            tParams.prevViewProjMatrix = m_PrevViewProjMatrix;
            tParams.screenSize = glm::vec4(
                static_cast<float>(m_RenderDevice.GetRenderWidth()),
                static_cast<float>(m_RenderDevice.GetRenderHeight()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderWidth()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderHeight())
            );
            // On first frame (or first few frames), don't blend with history (it's uninitialized)
            // Use more frames to allow temporal accumulation to stabilize
            tParams.temporalBlend = (m_FrameIndex < 8) ? 0.0f : m_RenderDevice.GetSSGITemporalBlend();
            tParams.depthThreshold = 0.05f;
            tParams.normalThreshold = 0.95f;
            tParams.useVelocity = 0;
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &tParams, sizeof(tParams));
            
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            m_Stats.drawCalls++;
            
            SDL_EndGPURenderPass(pass);
        }
        
        // ========== Pass 3: Spatial Denoising (Horizontal) ==========
        if (m_SSGIDenoisePipeline) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = ssgiTexture;  // Ping-pong: write to ssgiTexture
            colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_BindGPUGraphicsPipeline(pass, m_SSGIDenoisePipeline);
            
            SDL_GPUTextureSamplerBinding texBindings[3] = {};
            texBindings[0].texture = ssgiDenoiseTexture;  // Read from temporal output
            texBindings[0].sampler = m_LinearSampler;
            texBindings[1].texture = depthTexture;
            texBindings[1].sampler = m_LinearSampler;
            texBindings[2].texture = depthTexture;
            texBindings[2].sampler = m_LinearSampler;
            
            SDL_BindGPUFragmentSamplers(pass, 0, texBindings, 3);
            
            struct DenoiseParams {
                glm::vec4 screenSize;
                float depthSigma;
                float normalSigma;
                float colorSigma;
                int32_t kernelRadius;
                int32_t passIndex;
                int32_t _pad0;
                int32_t _pad1;
                int32_t _pad2;
            } dParams;
            
            dParams.screenSize = glm::vec4(
                static_cast<float>(m_RenderDevice.GetRenderWidth()),
                static_cast<float>(m_RenderDevice.GetRenderHeight()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderWidth()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderHeight())
            );
            dParams.depthSigma = 0.5f;
            dParams.normalSigma = 0.5f;
            dParams.colorSigma = 0.5f;
            dParams.kernelRadius = 3;
            dParams.passIndex = 0;  // Horizontal
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &dParams, sizeof(dParams));
            
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            m_Stats.drawCalls++;
            
            SDL_EndGPURenderPass(pass);
        }
        
        // ========== Pass 4: Spatial Denoising (Vertical) ==========
        if (m_SSGIDenoisePipeline) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = ssgiDenoiseTexture;  // Final output
            colorTarget.load_op = SDL_GPU_LOADOP_DONT_CARE;
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_BindGPUGraphicsPipeline(pass, m_SSGIDenoisePipeline);
            
            SDL_GPUTextureSamplerBinding texBindings[3] = {};
            texBindings[0].texture = ssgiTexture;  // Read from horizontal pass
            texBindings[0].sampler = m_LinearSampler;
            texBindings[1].texture = depthTexture;
            texBindings[1].sampler = m_LinearSampler;
            texBindings[2].texture = depthTexture;
            texBindings[2].sampler = m_LinearSampler;
            
            SDL_BindGPUFragmentSamplers(pass, 0, texBindings, 3);
            
            struct DenoiseParams {
                glm::vec4 screenSize;
                float depthSigma;
                float normalSigma;
                float colorSigma;
                int32_t kernelRadius;
                int32_t passIndex;
                int32_t _pad0;
                int32_t _pad1;
                int32_t _pad2;
            } dParams;
            
            dParams.screenSize = glm::vec4(
                static_cast<float>(m_RenderDevice.GetRenderWidth()),
                static_cast<float>(m_RenderDevice.GetRenderHeight()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderWidth()),
                1.0f / static_cast<float>(m_RenderDevice.GetRenderHeight())
            );
            dParams.depthSigma = 0.5f;
            dParams.normalSigma = 0.5f;
            dParams.colorSigma = 0.5f;
            dParams.kernelRadius = 3;
            dParams.passIndex = 1;  // Vertical
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &dParams, sizeof(dParams));
            
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            m_Stats.drawCalls++;
            
            SDL_EndGPURenderPass(pass);
        }
        
        // ========== Pass 5: Composite GI with Scene ==========
        // We use additive blending to add GI to the scene that's already in hdrTexture
        // This avoids the read-after-write hazard since we only sample from ssgiDenoiseTexture
        if (m_SSGICompositePipeline) {
            SDL_GPUColorTargetInfo colorTarget = {};
            colorTarget.texture = hdrTexture;  // Write back to HDR texture
            colorTarget.load_op = SDL_GPU_LOADOP_LOAD;  // Preserve existing content
            colorTarget.store_op = SDL_GPU_STOREOP_STORE;
            
            SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmdBuffer, &colorTarget, 1, nullptr);
            if (!pass) return;
            
            SDL_BindGPUGraphicsPipeline(pass, m_SSGICompositePipeline);
            
            // Only sample from SSGI result - the scene is loaded via LOAD_OP
            SDL_GPUTextureSamplerBinding texBindings[2] = {};
            texBindings[0].texture = ssgiDenoiseTexture; // Final GI
            texBindings[0].sampler = m_LinearSampler;
            texBindings[1].texture = ssgiDenoiseTexture; // Duplicate for padding (we use 2 in pipeline but only need 1)
            texBindings[1].sampler = m_LinearSampler;
            
            SDL_BindGPUFragmentSamplers(pass, 0, texBindings, 2);
            
            struct CompositeParams {
                float giIntensity;
                float aoStrength;
                int32_t debugMode;
                int32_t _pad0;
            } cParams;
            
            cParams.giIntensity = m_RenderDevice.GetSSGIIntensity();
            cParams.aoStrength = 0.0f;
            cParams.debugMode = m_RenderDevice.GetSSGIDebugMode();
            
            SDL_PushGPUFragmentUniformData(cmdBuffer, 0, &cParams, sizeof(cParams));
            
            SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
            m_Stats.drawCalls++;
            
            SDL_EndGPURenderPass(pass);
        }
        
        // Copy current SSGI result to history buffer for next frame's temporal pass
        // We use ssgiDenoiseTexture as the final result, copy it to history
        if (ssgiHistoryTexture && ssgiDenoiseTexture) {
            SDL_GPUBlitInfo blitInfo = {};
            blitInfo.source.texture = ssgiDenoiseTexture;
            blitInfo.source.w = m_RenderDevice.GetRenderWidth() / 2;
            blitInfo.source.h = m_RenderDevice.GetRenderHeight() / 2;
            blitInfo.destination.texture = ssgiHistoryTexture;
            blitInfo.destination.w = m_RenderDevice.GetRenderWidth() / 2;
            blitInfo.destination.h = m_RenderDevice.GetRenderHeight() / 2;
            blitInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;
            blitInfo.filter = SDL_GPU_FILTER_LINEAR;
            
            SDL_BlitGPUTexture(cmdBuffer, &blitInfo);
        }
        
        // Update matrices for next frame
        m_PrevViewProjMatrix = viewProj;
        m_FrameIndex++;
    }

    void RenderSystem::EndFrame() {
        // SSGI pass - runs after main scene rendering, before bloom
        if (m_RenderDevice.IsSSGIEnabled() && m_RenderDevice.IsHDREnabled()) {
            RenderSSGIPass(m_CurrentView, m_CurrentProj);
        }
        
        // If HDR is enabled with bloom, run bloom pass first
        if (m_RenderDevice.IsHDREnabled() && m_RenderDevice.IsBloomEnabled()) {
            RenderBloomPass();
        }
        
        // Run tone mapping pass (includes bloom compositing if bloom was run)
        // This leaves the render pass OPEN so UI can be drawn on top
        if (m_RenderDevice.IsHDREnabled() && m_ToneMappingPipeline) {
            RenderToneMappingPass();
        }
        // Note: Render pass is still open - call FinishFrame() after UI rendering
    }

    void RenderSystem::FinishFrame() {
        // End render pass and submit command buffer
        m_RenderDevice.EndFrame();
    }

}
