#pragma once

#include <SDL3/SDL.h>

namespace Platform {

    class Window;

    // Tone mapping operators
    enum class ToneMapOperator {
        Reinhard = 0,
        ACES = 1,
        Uncharted2 = 2
    };

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
        SDL_GPUTextureFormat GetSwapchainTextureFormat() const;

        SDL_GPUTexture* CreateTexture(uint32_t width, uint32_t height, const void* data);
        
        // HDR Pipeline
        bool IsHDREnabled() const { return m_HDREnabled; }
        void SetHDREnabled(bool enabled) { m_HDREnabled = enabled; }
        SDL_GPUTexture* GetHDRTexture() const { return m_HDRTexture; }
        SDL_GPUTexture* GetSwapchainTexture() const { return m_SwapchainTexture; }
        SDL_GPUTexture* GetDepthTexture() const { return m_DepthTexture; }
        uint32_t GetRenderWidth() const { return m_RenderWidth; }
        uint32_t GetRenderHeight() const { return m_RenderHeight; }
        
        // Tone mapping settings
        float GetExposure() const { return m_Exposure; }
        void SetExposure(float exposure) { m_Exposure = exposure; }
        float GetGamma() const { return m_Gamma; }
        void SetGamma(float gamma) { m_Gamma = gamma; }
        ToneMapOperator GetToneMapOperator() const { return m_ToneMapOperator; }
        void SetToneMapOperator(ToneMapOperator op) { m_ToneMapOperator = op; }
        
        // Frame validity - false if window is minimized or swapchain unavailable
        bool IsFrameValid() const { return m_FrameValid; }
        
        void BeginFrame();
        bool BeginRenderPass(); // Returns true if successful (renders to HDR target if enabled)
        void EndRenderPass();   // Ends the main render pass (so tone mapping can start a new one)
        bool BeginToneMappingPass();  // Starts render pass to swapchain for tone mapping
        void EndFrame();

    private:
        void CreateDepthTexture(uint32_t width, uint32_t height);
        void CreateHDRTexture(uint32_t width, uint32_t height);

        SDL_GPUDevice* m_Device = nullptr;
        Window* m_Window = nullptr;
        SDL_GPUCommandBuffer* m_CommandBuffer = nullptr;
        SDL_GPURenderPass* m_RenderPass = nullptr;
        SDL_GPUTexture* m_SwapchainTexture = nullptr;
        SDL_GPUTexture* m_DepthTexture = nullptr;
        SDL_GPUTexture* m_HDRTexture = nullptr;
        uint32_t m_DepthWidth = 0;
        uint32_t m_DepthHeight = 0;
        uint32_t m_HDRWidth = 0;
        uint32_t m_HDRHeight = 0;
        uint32_t m_RenderWidth = 0;
        uint32_t m_RenderHeight = 0;
        
        // HDR settings
        bool m_HDREnabled = true;  // Enable HDR by default
        float m_Exposure = 1.0f;
        float m_Gamma = 2.2f;
        ToneMapOperator m_ToneMapOperator = ToneMapOperator::ACES;
        
        // Frame validity
        bool m_FrameValid = false;
    };

}
