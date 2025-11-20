#pragma once

#include "../Core/Context.h"
#include "../Platform/RenderDevice.h"
#include <SDL3/SDL.h>

namespace Systems {

    class RenderSystem {
    public:
        RenderSystem(Core::GameContext& context, Platform::RenderDevice& renderDevice);
        ~RenderSystem();
        
        void Init();
        void BeginFrame();
        void DrawScene(double alpha);
        void EndFrame();

    private:
        Core::GameContext& m_Context;
        Platform::RenderDevice& m_RenderDevice;
        
        SDL_GPUGraphicsPipeline* m_Pipeline = nullptr;
        SDL_GPUSampler* m_Sampler = nullptr;
        
        void CreatePipeline();
    };

}
