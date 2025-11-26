#pragma once

#include "../Core/Context.h"
#include "../Platform/RenderDevice.h"
#include "../Resources/ResourceManager.h"
#include <SDL3/SDL.h>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Systems {

    struct LineVertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    class RenderSystem {
    public:
        RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice, Resources::ResourceManager& resourceManager);
        ~RenderSystem();
        
        void Init();
        void BeginFrame();
        void DrawScene(double alpha);
        void EndFrame();

        void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);
        
        // Debug drawing for physics visualization
        void DrawWireBox(const glm::vec3& center, const glm::vec3& halfExtents, const glm::vec3& color, const glm::quat& rotation = glm::quat(1,0,0,0));
        void DrawWireSphere(const glm::vec3& center, float radius, const glm::vec3& color, int segments = 16);
        void DrawWireCapsule(const glm::vec3& center, float halfHeight, float radius, const glm::vec3& color, int segments = 12);
        void DrawPhysicsDebug(); // Draw all colliders in the scene

    private:
        Core::GameContext& m_Context;
        Platform::RenderDevice& m_RenderDevice;
        Resources::ResourceManager& m_ResourceManager;
        
        SDL_GPUGraphicsPipeline* m_Pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_MeshPipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_LinePipeline = nullptr;
        SDL_GPUSampler* m_Sampler = nullptr;
        
        std::vector<LineVertex> m_LineVertices;
        std::vector<SDL_GPUBuffer*> m_BuffersToDelete;
        std::vector<SDL_GPUTransferBuffer*> m_TransferBuffersToDelete;
        SDL_GPUBuffer* m_CurrentLineBuffer = nullptr;
        SDL_GPUBuffer* m_DefaultSkinBuffer = nullptr;

        void CreatePipeline();
        void CreateMeshPipeline();
        void CreateLinePipeline();
    };

}
