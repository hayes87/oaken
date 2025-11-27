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
        void EndFrame();

        void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);
        
        // Debug drawing for physics visualization
        void DrawWireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, const glm::quat& rotation = glm::quat(1,0,0,0));
        void DrawWireSphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 16);
        void DrawWireCapsule(const glm::vec3& center, float halfHeight, float radius, const glm::vec3& color, int segments = 12);
        void DrawPhysicsDebug(); // Draw all colliders in the scene
        
        // Render statistics
        const RenderStats& GetStats() const { return m_Stats; }

    private:
        Core::GameContext& m_Context;
        Platform::RenderDevice& m_RenderDevice;
        Resources::ResourceManager& m_ResourceManager;
        
        SDL_GPUGraphicsPipeline* m_Pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_MeshPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_InstancedMeshPipeline = nullptr;  // For batch rendering
        SDL_GPUGraphicsPipeline* m_LinePipeline = nullptr;
        SDL_GPUSampler* m_Sampler = nullptr;
        
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
        
        void BuildBatches();
        void RenderBatches(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj, const void* lightUbo, size_t lightUboSize);
        void RenderSkinnedMeshes(SDL_GPURenderPass* pass, const glm::mat4& view, const glm::mat4& proj, const void* lightUbo, size_t lightUboSize,
                                  const std::unordered_map<uint64_t, std::vector<glm::mat4>>& skinData);
    };

}
