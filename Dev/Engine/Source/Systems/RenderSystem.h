#pragma once

#include "../Core/Context.h"
#include "../Platform/RenderDevice.h"
#include "../Resources/ResourceManager.h"
#include <SDL3/SDL.h>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Resources { class Mesh; }

namespace Systems {

    struct LineVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    // Instance data for batch rendering (static meshes)
    struct MeshInstance {
        glm::mat4 model;
        glm::vec4 color;
    };

    // Batch of instances sharing the same mesh (static meshes only)
    struct MeshBatch {
        std::shared_ptr<Resources::Mesh> mesh;
        std::vector<MeshInstance> instances;
        uint32_t instanceOffset = 0;  // Offset into shared instance buffer
    };

    // Render statistics
    struct RenderStats {
        uint32_t drawCalls = 0;
        uint32_t totalInstances = 0;
        uint32_t batchedInstances = 0;
        uint32_t skinnedInstances = 0;
        uint32_t lineVertices = 0;
        
        void Reset() {
            drawCalls = 0;
            totalInstances = 0;
            batchedInstances = 0;
            skinnedInstances = 0;
            lineVertices = 0;
        }
    };

    class RenderSystem {
    public:
        RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice, Resources::ResourceManager& resourceManager);
        ~RenderSystem();
        
        void Init();
        void BeginFrame(bool drawSkeleton = true);
        void DrawScene(double alpha);
        void EndFrame();      // Runs bloom + tone mapping, leaves render pass open for UI
        void FinishFrame();   // Ends render pass and submits command buffer

        void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);
        
        // Debug drawing for physics visualization
        void DrawWireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, const glm::quat& rotation = glm::quat(1,0,0,0));
        void DrawWireSphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 16);
        void DrawWireCapsule(const glm::vec3& center, float halfHeight, float radius, const glm::vec3& color, int segments = 12);
        void DrawPhysicsDebug(); // Draw all colliders in the scene
        
        // Render statistics
        const RenderStats& GetStats() const { return m_Stats; }
        
        // Shadow mapping - expose light-space matrix for debug/external use
        const glm::mat4& GetLightSpaceMatrix() const { return m_LightSpaceMatrix; }

    private:
        Core::GameContext& m_Context;
        Platform::RenderDevice& m_RenderDevice;
        Resources::ResourceManager& m_ResourceManager;
        
        SDL_GPUGraphicsPipeline* m_Pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_MeshPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_InstancedMeshPipeline = nullptr;  // For batch rendering
        SDL_GPUGraphicsPipeline* m_LinePipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_ToneMappingPipeline = nullptr;    // For HDR tone mapping
        SDL_GPUSampler* m_Sampler = nullptr;
        SDL_GPUSampler* m_LinearSampler = nullptr;  // For HDR texture sampling
        
        std::vector<LineVertex> m_LineVertices;
        std::vector<SDL_GPUBuffer*> m_BuffersToDelete;
        std::vector<SDL_GPUTransferBuffer*> m_TransferBuffersToDelete;
        SDL_GPUBuffer* m_CurrentLineBuffer = nullptr;
        SDL_GPUBuffer* m_DefaultSkinBuffer = nullptr;
        
        // Batch rendering
        std::unordered_map<Resources::Mesh*, MeshBatch> m_Batches;
        SDL_GPUBuffer* m_InstanceBuffer = nullptr;
        uint32_t m_InstanceBufferCapacity = 0;
        RenderStats m_Stats;

        void CreatePipeline();
        void CreateMeshPipeline();
        void CreateInstancedMeshPipeline();
        void CreateLinePipeline();
        void CreateToneMappingPipeline();
        void CreateDepthOnlyPipeline();
        void CreateLightCullingPipeline();
        void CreateShadowMapPipeline();
        
        void RenderToneMappingPass();  // Tone map HDR -> Swapchain
        void RenderDepthPrePass(const glm::mat4& view, const glm::mat4& proj);  // Depth pre-pass for Forward+
        void RenderShadowPass(const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData);  // Render shadow map from light's perspective
        void DispatchLightCulling(const glm::mat4& view, const glm::mat4& proj);  // Light culling compute
        void UpdateLightBufferForForwardPlus();  // Update GPU light buffer for Forward+ culling
        
        void BuildBatches();
        void RenderBatches(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj, const void* lightUbo, size_t lightUboSize);
        void RenderSkinnedMeshes(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj, const void* lightUbo, size_t lightUboSize,
                                  const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData);
        
        // Forward+ pipelines
        SDL_GPUGraphicsPipeline* m_DepthOnlyPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_ForwardPlusPipeline = nullptr;  // Forward+ instanced mesh pipeline
        SDL_GPUComputePipeline* m_LightCullingPipeline = nullptr;
        SDL_GPUSampler* m_DepthSampler = nullptr;  // For sampling depth in compute
        
        // Shadow mapping
        SDL_GPUGraphicsPipeline* m_ShadowMapPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_ShadowMapSkinnedPipeline = nullptr;  // For skinned meshes
        glm::mat4 m_LightSpaceMatrix = glm::mat4(1.0f);  // Cached for fragment shader
        void CreateShadowMapSkinnedPipeline();
        void RenderSkinnedMeshesToShadowMap(SDL_GPURenderPass* pass, const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData);
        
        // Bloom pipelines
        SDL_GPUGraphicsPipeline* m_BloomBrightPassPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_BloomBlurPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_BloomCompositePipeline = nullptr;
        SDL_GPUTexture* m_BloomResultTexture = nullptr;  // Points to final blurred bloom texture
        
        // Forward+ rendering method
        void CreateForwardPlusPipeline();
        void RenderBatchesForwardPlus(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj);
        
        // Bloom rendering methods
        void CreateBloomPipelines();
        void RenderBloomPass();  // Full bloom pass (bright extract + blur + composite)
    };

}
