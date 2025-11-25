#include "RenderSystem.h"
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>
#include "../Components/Components.h"
#include "../Core/Log.h"
#include "../Resources/Mesh.h"
#include "../Resources/Texture.h"
#include "../Resources/Shader.h"
#include "../Platform/Window.h"

namespace Systems {

    RenderSystem::RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice, Resources::ResourceManager& resourceManager)
        : m_Context(context), m_RenderDevice(renderDevice), m_ResourceManager(resourceManager) {}

    RenderSystem::~RenderSystem() {
        if (m_Pipeline) {
            SDL_ReleaseGPUGraphicsPipeline(m_RenderDevice.GetDevice(), m_Pipeline);
        }
        if (m_Sampler) {
            SDL_ReleaseGPUSampler(m_RenderDevice.GetDevice(), m_Sampler);
        }
    }

    void RenderSystem::Init() {
        CreatePipeline();
        CreateMeshPipeline();
        
        SDL_GPUSamplerCreateInfo samplerInfo = {};
        samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        
        m_Sampler = SDL_CreateGPUSampler(m_RenderDevice.GetDevice(), &samplerInfo);
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

        // Color Blend
        SDL_GPUColorTargetDescription colorTargetDesc = {0};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
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

        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 1);
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 0);

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

        // Joints
        vertexAttributes[4].location = 4;
        vertexAttributes[4].buffer_slot = 0;
        vertexAttributes[4].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT4;
        vertexAttributes[4].offset = offsetof(Resources::Vertex, joints);

        pipelineInfo.vertex_input_state.num_vertex_buffers = 1;
        pipelineInfo.vertex_input_state.vertex_buffer_descriptions = &vertexBufferDesc;
        pipelineInfo.vertex_input_state.num_vertex_attributes = 5;
        pipelineInfo.vertex_input_state.vertex_attributes = vertexAttributes;

        // Color Target
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
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

    void RenderSystem::BeginFrame() {
        m_RenderDevice.BeginFrame();
    }

    void RenderSystem::DrawScene(double alpha) {
        (void)alpha;
        
        if (!m_Pipeline) return;

        SDL_GPURenderPass* pass = m_RenderDevice.GetRenderPass();
        if (pass) {
            // Draw Sprites
            SDL_BindGPUGraphicsPipeline(pass, m_Pipeline);
            
            m_Context.World->query<WorldTransform, SpriteComponent>()
                .each([&](flecs::entity e, WorldTransform& t, SpriteComponent& s) {
                if (s.texture) {
                    // Use computed World Matrix
                    glm::mat4 model = t.matrix;

                    // Push Uniform Data (Slot 0, Set 1 in shader)
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

                // Camera Matrices (View/Proj)
                glm::mat4 view = glm::mat4(1.0f);
                glm::mat4 proj = glm::mat4(1.0f);
                
                // Find Primary Camera
                bool cameraFound = false;
                m_Context.World->query<LocalTransform, const CameraComponent>()
                    .each([&](flecs::entity e, LocalTransform& t, const CameraComponent& cam) {
                        if (cam.isPrimary && !cameraFound) {
                            // View Matrix (Inverse of Camera Transform)
                            // Reconstruct camera matrix from position and rotation
                            glm::mat4 camMatrix = glm::translate(glm::mat4(1.0f), t.position);
                            camMatrix = glm::rotate(camMatrix, glm::radians(t.rotation.y), glm::vec3(0, 1, 0));
                            camMatrix = glm::rotate(camMatrix, glm::radians(t.rotation.x), glm::vec3(1, 0, 0));
                            camMatrix = glm::rotate(camMatrix, glm::radians(t.rotation.z), glm::vec3(0, 0, 1));
                            
                            view = glm::inverse(camMatrix);

                            // Projection Matrix
                            proj = glm::perspectiveRH_ZO(glm::radians(cam.fov), 1280.0f / 720.0f, cam.nearPlane, cam.farPlane);
                            // proj[1][1] *= -1; // Vulkan clip space correction - Removed as user reported inverted Y
                            
                            cameraFound = true;
                        }
                    });

                if (!cameraFound) {
                    // Fallback camera
                    view = glm::lookAt(glm::vec3(0, 2, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
                    proj = glm::perspectiveRH_ZO(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
                    // proj[1][1] *= -1;
                }

                m_Context.World->query<WorldTransform, MeshComponent>()
                    .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp) {
                    if (meshComp.mesh) {
                        struct UBO {
                            glm::mat4 model;
                            glm::mat4 view;
                            glm::mat4 proj;
                            glm::mat4 jointMatrices[256];
                        } ubo;
                        
                        ubo.model = t.matrix;
                        ubo.view = view;
                        ubo.proj = proj;

                        // Skinning
                        const AnimatorComponent* animator = nullptr;
                        if (e.has<AnimatorComponent>()) {
                            animator = &e.get<AnimatorComponent>();
                        }
                        
                        if (animator && !animator->models.empty()) {
                            size_t count = std::min(animator->models.size(), (size_t)256);
                            
                            const auto& ibms = meshComp.mesh->GetInverseBindMatrices();
                            if (ibms.size() >= count) {
                                for (size_t i = 0; i < count; ++i) {
                                    glm::mat4 modelTransform;
                                    memcpy(&modelTransform, &animator->models[i], sizeof(glm::mat4));
                                    ubo.jointMatrices[i] = modelTransform * ibms[i];
                                }
                                
                                // Fill rest with identity to be safe
                                for (size_t i = count; i < 256; ++i) ubo.jointMatrices[i] = glm::mat4(1.0f);
                            }
                        } else {
                            for (int i = 0; i < 256; ++i) ubo.jointMatrices[i] = glm::mat4(1.0f);
                        }

                        SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 0, &ubo, sizeof(ubo));
                        
                        // Bind Vertex Buffers

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
                    }
                });
            }
        }
    }

    void RenderSystem::EndFrame() {
        m_RenderDevice.EndFrame();
    }

}
