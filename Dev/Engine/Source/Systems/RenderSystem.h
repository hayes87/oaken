#pragma once

#include "../Core/Context.h"
#include "../Platform/RenderDevice.h"
#include "../Resources/ResourceManager.h"
#include <SDL3/SDL.h>

namespace Systems {

    class RenderSystem {
    public:
        RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice, Resources::ResourceManager& resourceManager);
        ~RenderSystem();
        
        void Init();
        void BeginFrame();
        void DrawScene(double alpha);
        void EndFrame();

    private:
        Core::GameContext& m_Context;
        Platform::RenderDevice& m_RenderDevice;
        Resources::ResourceManager& m_ResourceManager;
        
        SDL_GPUGraphicsPipeline* m_Pipeline = nullptr;
        SDL_GPUGraphicsPipeline* m_MeshPipeline = nullptr;
        SDL_GPUSampler* m_Sampler = nullptr;
        
        void CreatePipeline();
        void CreateMeshPipeline();
    };

}
