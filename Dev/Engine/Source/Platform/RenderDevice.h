#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

namespace Platform {

    class Window;

    // Tone mapping operators
    enum class ToneMapOperator {
        Reinhard = 0,
        ACES = 1,
        Uncharted2 = 2
    };

    // Forward+ rendering constants
    constexpr uint32_t FORWARD_PLUS_TILE_SIZE = 16;
    constexpr uint32_t MAX_LIGHTS_PER_TILE = 256;
    constexpr uint32_t MAX_POINT_LIGHTS = 1024;

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
        
        // Forward+ Pipeline
        bool IsForwardPlusEnabled() const { return m_ForwardPlusEnabled; }
        void SetForwardPlusEnabled(bool enabled) { m_ForwardPlusEnabled = enabled; }
        SDL_GPUBuffer* GetTileLightIndicesBuffer() const { return m_TileLightIndicesBuffer; }
        SDL_GPUBuffer* GetLightBuffer() const { return m_LightBuffer; }
        uint32_t GetNumTilesX() const { return m_NumTilesX; }
        uint32_t GetNumTilesY() const { return m_NumTilesY; }
        
        // Tone mapping settings
        float GetExposure() const { return m_Exposure; }
        void SetExposure(float exposure) { m_Exposure = exposure; }
        float GetGamma() const { return m_Gamma; }
        void SetGamma(float gamma) { m_Gamma = gamma; }
        ToneMapOperator GetToneMapOperator() const { return m_ToneMapOperator; }
        void SetToneMapOperator(ToneMapOperator op) { m_ToneMapOperator = op; }
        
        // Bloom settings
        bool IsBloomEnabled() const { return m_BloomEnabled; }
        void SetBloomEnabled(bool enabled) { m_BloomEnabled = enabled; }
        float GetBloomThreshold() const { return m_BloomThreshold; }
        void SetBloomThreshold(float threshold) { m_BloomThreshold = threshold; }
        float GetBloomIntensity() const { return m_BloomIntensity; }
        void SetBloomIntensity(float intensity) { m_BloomIntensity = intensity; }
        int GetBloomBlurPasses() const { return m_BloomBlurPasses; }
        void SetBloomBlurPasses(int passes) { m_BloomBlurPasses = passes; }
        
        // Bloom textures (ping-pong buffers for blur)
        SDL_GPUTexture* GetBloomBrightTexture() const { return m_BloomBrightTexture; }
        SDL_GPUTexture* GetBloomBlurTextureA() const { return m_BloomBlurTextureA; }
        SDL_GPUTexture* GetBloomBlurTextureB() const { return m_BloomBlurTextureB; }
        
        // Frame validity - false if window is minimized or swapchain unavailable
        bool IsFrameValid() const { return m_FrameValid; }
        
        void BeginFrame();
        bool BeginRenderPass(); // Returns true if successful (renders to HDR target if enabled)
        bool BeginDepthPrePass(); // Depth-only pre-pass for Forward+
        void EndRenderPass();   // Ends the main render pass (so tone mapping can start a new one)
        bool BeginToneMappingPass();  // Starts render pass to swapchain for tone mapping
        void EndFrame();
        
        // Forward+ light culling
        void UpdateLightBuffer(const void* lightData, uint32_t numLights);
        void DispatchLightCulling(SDL_GPUComputePipeline* cullingPipeline, const glm::mat4& view, const glm::mat4& proj);
        void EnsureForwardPlusBuffers();  // Create Forward+ buffers if they don't exist
        
        // Shadow mapping
        bool IsShadowsEnabled() const { return m_ShadowsEnabled; }
        void SetShadowsEnabled(bool enabled) { m_ShadowsEnabled = enabled; }
        uint32_t GetShadowMapSize() const { return m_ShadowMapSize; }
        void SetShadowMapSize(uint32_t size);
        float GetShadowBias() const { return m_ShadowBias; }
        void SetShadowBias(float bias) { m_ShadowBias = bias; }
        float GetShadowNormalBias() const { return m_ShadowNormalBias; }
        void SetShadowNormalBias(float bias) { m_ShadowNormalBias = bias; }
        int GetShadowPcfSamples() const { return m_ShadowPcfSamples; }
        void SetShadowPcfSamples(int samples) { m_ShadowPcfSamples = samples; }
        SDL_GPUTexture* GetShadowMapTexture() const { return m_ShadowMapTexture; }
        SDL_GPUSampler* GetShadowSampler() const { return m_ShadowSampler; }
        bool BeginShadowPass();  // Begin render pass for shadow map
        void EndShadowPass();    // End shadow pass

    private:
        void CreateDepthTexture(uint32_t width, uint32_t height);
        void CreateHDRTexture(uint32_t width, uint32_t height);
        void CreateForwardPlusBuffers(uint32_t width, uint32_t height);
        void CreateBloomTextures(uint32_t width, uint32_t height);
        void CreateShadowMapTexture(uint32_t size);

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
        
        // Bloom settings and textures
        bool m_BloomEnabled = true;
        float m_BloomThreshold = 0.8f;   // Brightness threshold for bloom
        float m_BloomIntensity = 0.3f;   // Subtle bloom strength
        int m_BloomBlurPasses = 5;       // Number of blur iterations
        SDL_GPUTexture* m_BloomBrightTexture = nullptr;  // Bright pixels extracted
        SDL_GPUTexture* m_BloomBlurTextureA = nullptr;   // Ping-pong blur A
        SDL_GPUTexture* m_BloomBlurTextureB = nullptr;   // Ping-pong blur B
        uint32_t m_BloomWidth = 0;   // Bloom texture dimensions (can be half res)
        uint32_t m_BloomHeight = 0;
        
        // Forward+ settings and buffers
        bool m_ForwardPlusEnabled = true;  // Enable Forward+ rendering
        SDL_GPUBuffer* m_TileLightIndicesBuffer = nullptr;
        SDL_GPUBuffer* m_LightBuffer = nullptr;
        SDL_GPUSampler* m_DepthSampler = nullptr;  // For sampling depth in compute
        uint32_t m_NumTilesX = 0;
        uint32_t m_NumTilesY = 0;
        uint32_t m_TileBufferSize = 0;
        
        // Shadow mapping
        bool m_ShadowsEnabled = true;  // Shadows enabled by default
        uint32_t m_ShadowMapSize = 4096;     // Higher res shadow map
        float m_ShadowBias = 0.005f;         // Depth bias
        float m_ShadowNormalBias = 0.001f;   // Slope-based bias
        int m_ShadowPcfSamples = 1;          // PCF kernel size (0=hard, 1=3x3, 2=5x5)
        SDL_GPUTexture* m_ShadowMapTexture = nullptr;
        SDL_GPUSampler* m_ShadowSampler = nullptr;  // Comparison sampler for PCF
        
        // Frame validity
        bool m_FrameValid = false;
    };

}
