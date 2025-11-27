#include "RenderSystem.h"
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <flecs.h>
#include <iostream>
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
        if (m_Sampler) {
            SDL_ReleaseGPUSampler(m_RenderDevice.GetDevice(), m_Sampler);
        }
        if (m_DefaultSkinBuffer) {
            SDL_ReleaseGPUBuffer(m_RenderDevice.GetDevice(), m_DefaultSkinBuffer);
        }
    }

    void RenderSystem::Init() {
        CreatePipeline();
        CreateMeshPipeline();
        CreateLinePipeline();
        
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

        // Vertex Shader: 0 Samplers, 0 Storage Textures, 0 Storage Buffers, 2 Uniform Buffers (Scene + Skin)
        auto vertShader = m_ResourceManager.LoadShader(vertPath, SDL_GPU_SHADERSTAGE_VERTEX, 0, 0, 0, 2);
        // Fragment Shader: 0 Samplers, 0 Storage Textures, 0 Storage Buffers, 1 Uniform Buffer (Lights)
        auto fragShader = m_ResourceManager.LoadShader(fragPath, SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0, 0, 1);

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

        // Color Target
        SDL_GPUColorTargetDescription colorTargetDesc = {};
        colorTargetDesc.format = SDL_GetGPUSwapchainTextureFormat(device, m_RenderDevice.GetWindow()->GetNativeWindow());
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

    void RenderSystem::BeginFrame() {
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
        
        // Debug Draw Skeletons
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

        // Prepare Skinning Data (UBO-based)
        std::unordered_map<uint64_t, std::vector<glm::mat4>> skinData;
        
        m_Context.World->query<WorldTransform, MeshComponent, AnimatorComponent>()
            .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp, AnimatorComponent& anim) {
                if (meshComp.mesh && !anim.models.empty()) {
                    const auto& compactIBMs = meshComp.mesh->GetInverseBindMatrices();
                    const auto& jointRemaps = meshComp.mesh->GetJointRemaps();
                    size_t usedJointCount = jointRemaps.size();
                    
                    std::vector<glm::mat4> jointMatrices(256, glm::mat4(1.0f));
                    
                    // Compute: skinningMatrix[compactIdx] = models[joint_remaps[compactIdx]] * ibms[compactIdx]
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

        // Copy Pass for Lines
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

        SDL_EndGPUCopyPass(copyPass);

        // 2. Begin Render Pass
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

                // Camera Matrices (View/Proj)
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
                
                // Query point lights
                int pointLightIdx = 0;
                m_Context.World->query<const WorldTransform, const PointLight>()
                    .each([&](flecs::entity e, const WorldTransform& t, const PointLight& light) {
                        if (pointLightIdx < MAX_POINT_LIGHTS) {
                            glm::vec3 pos = glm::vec3(t.matrix[3]);
                            lightUbo.pointLightPos[pointLightIdx] = glm::vec4(pos, light.radius);
                            lightUbo.pointLightColor[pointLightIdx] = glm::vec4(light.color, light.intensity);
                            pointLightIdx++;
                        }
                    });
                lightUbo.numPointLights = pointLightIdx;

                m_Context.World->query<WorldTransform, MeshComponent>()
                    .each([&](flecs::entity e, WorldTransform& t, MeshComponent& meshComp) {
                    if (meshComp.mesh) {
                        struct SceneUBO {
                            glm::mat4 model;
                            glm::mat4 view;
                            glm::mat4 proj;
                        } sceneUbo;
                        
                        // Apply render offset for mesh origin adjustment
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
                        if (skinData.count(e.id()) > 0) {
                            skinMatrices = skinData[e.id()];
                        }
                        SDL_PushGPUVertexUniformData(m_RenderDevice.GetCommandBuffer(), 1, skinMatrices.data(), 256 * sizeof(glm::mat4));
                        
                        // Push Light UBO to fragment shader
                        SDL_PushGPUFragmentUniformData(m_RenderDevice.GetCommandBuffer(), 0, &lightUbo, sizeof(lightUbo));
                        
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

    void RenderSystem::EndFrame() {
        m_RenderDevice.EndFrame();
    }

}
