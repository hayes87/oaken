#pragma once

#include <SDL3/SDL.h>

namespace Platform {

    class Window;

    class RenderDevice {
    public:
        RenderDevice();
        ~RenderDevice();

        bool Init(Window* window);
        void Shutdown();

        SDL_GPUDevice* GetDevice() const { return m_Device; }
        Window* GetWindow() const { return m_Window; }
        SDL_GPUCommandBuffer* GetCommandBuffer() const { return m_CommandBuffer; }
        SDL_GPURenderPass* GetRenderPass() const { return m_RenderPass; }

        SDL_GPUTexture* CreateTexture(uint32_t width, uint32_t height, const void* data);
        
        void BeginFrame();
        void EndFrame();

    private:
        void CreateDepthTexture(uint32_t width, uint32_t height);

        SDL_GPUDevice* m_Device = nullptr;
        Window* m_Window = nullptr;
        SDL_GPUCommandBuffer* m_CommandBuffer = nullptr;
        SDL_GPURenderPass* m_RenderPass = nullptr;
        SDL_GPUTexture* m_SwapchainTexture = nullptr;
        SDL_GPUTexture* m_DepthTexture = nullptr;
    };

}
