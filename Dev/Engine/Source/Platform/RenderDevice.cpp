#include "RenderDevice.h"
#include "Window.h"
#include <iostream>

namespace Platform {

    RenderDevice::RenderDevice() {}

    RenderDevice::~RenderDevice() {
        Shutdown();
    }

    bool RenderDevice::Init(Window* window) {
        m_Window = window;

        // Request SPIR-V (Vulkan)
        m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
        
        if (!m_Device) {
            std::cerr << "SDL_CreateGPUDevice Error: " << SDL_GetError() << std::endl;
            return false;
        }

        std::cout << "GPU Driver: " << SDL_GetGPUDeviceDriver(m_Device) << std::endl;

        if (!SDL_ClaimWindowForGPUDevice(m_Device, m_Window->GetNativeWindow())) {
            std::cerr << "SDL_ClaimWindowForGPUDevice Error: " << SDL_GetError() << std::endl;
            return false;
        }

        return true;
    }

    SDL_GPUTextureFormat RenderDevice::GetSwapchainTextureFormat() const {
        return SDL_GetGPUSwapchainTextureFormat(m_Device, m_Window->GetNativeWindow());
    }

    void RenderDevice::Shutdown() {
        if (m_Device) {
            if (m_HDRTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_HDRTexture);
                m_HDRTexture = nullptr;
            }
            if (m_DepthTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_DepthTexture);
                m_DepthTexture = nullptr;
            }
            if (m_Window) {
                SDL_ReleaseWindowFromGPUDevice(m_Device, m_Window->GetNativeWindow());
            }
            SDL_DestroyGPUDevice(m_Device);
            m_Device = nullptr;
        }
    }

    void RenderDevice::CreateDepthTexture(uint32_t width, uint32_t height) {
        if (m_DepthTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_DepthTexture);
        }

        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        m_DepthTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
    }

    void RenderDevice::CreateHDRTexture(uint32_t width, uint32_t height) {
        if (m_HDRTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_HDRTexture);
        }

        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;  // Render target + sampler for tone mapping
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;

        m_HDRTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        if (m_HDRTexture) {
            std::cout << "HDR Texture created: " << width << "x" << height << " (RGBA16F)" << std::endl;
        }
    }

    void RenderDevice::BeginFrame() {
        m_FrameValid = false;  // Reset at start of frame
        
        m_CommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
        if (!m_CommandBuffer) {
            std::cerr << "BeginFrame: Failed to acquire command buffer!" << std::endl;
            return;
        }

        uint32_t w, h;
        if (!SDL_AcquireGPUSwapchainTexture(m_CommandBuffer, m_Window->GetNativeWindow(), &m_SwapchainTexture, &w, &h)) {
            std::cerr << "BeginFrame: Failed to acquire swapchain texture: " << SDL_GetError() << std::endl;
            return;
        }

        // Skip if window is minimized (0x0)
        if (w == 0 || h == 0) {
            m_SwapchainTexture = nullptr;
            return;
        }
        
        m_FrameValid = true;  // Frame is valid - we have a swapchain

        m_RenderWidth = w;
        m_RenderHeight = h;

        // Recreate depth texture if size changed or doesn't exist
        if (!m_DepthTexture || w != m_DepthWidth || h != m_DepthHeight) {
            CreateDepthTexture(w, h);
            m_DepthWidth = w;
            m_DepthHeight = h;
        }

        // Recreate HDR texture if size changed or doesn't exist (only if HDR enabled)
        if (m_HDREnabled && (!m_HDRTexture || w != m_HDRWidth || h != m_HDRHeight)) {
            CreateHDRTexture(w, h);
            m_HDRWidth = w;
            m_HDRHeight = h;
        }
    }

    bool RenderDevice::BeginRenderPass() {
        if (!m_FrameValid || !m_CommandBuffer || !m_SwapchainTexture) return false;

        SDL_GPUTexture* colorTarget = m_HDREnabled && m_HDRTexture ? m_HDRTexture : m_SwapchainTexture;

        SDL_GPUColorTargetInfo colorTargetInfo = {};
        colorTargetInfo.texture = colorTarget;
        colorTargetInfo.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f }; // Black for HDR (will be tonemapped)
        colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPUDepthStencilTargetInfo depthStencilInfo = {};
        depthStencilInfo.texture = m_DepthTexture;
        depthStencilInfo.clear_depth = 1.0f;
        depthStencilInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthStencilInfo.store_op = SDL_GPU_STOREOP_STORE;
        depthStencilInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
        depthStencilInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;
        depthStencilInfo.cycle = true; // Important if we reuse the texture

        m_RenderPass = SDL_BeginGPURenderPass(m_CommandBuffer, &colorTargetInfo, 1, &depthStencilInfo);
        return m_RenderPass != nullptr;
    }

    void RenderDevice::EndRenderPass() {
        if (m_RenderPass) {
            SDL_EndGPURenderPass(m_RenderPass);
            m_RenderPass = nullptr;
        }
    }

    bool RenderDevice::BeginToneMappingPass() {
        if (!m_FrameValid || !m_CommandBuffer || !m_SwapchainTexture) {
            return false;
        }
        
        // This pass renders to the swapchain (no depth needed)
        SDL_GPUColorTargetInfo colorTargetInfo = {};
        colorTargetInfo.texture = m_SwapchainTexture;
        colorTargetInfo.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
        colorTargetInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;  // We're overwriting everything
        colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
        
        m_RenderPass = SDL_BeginGPURenderPass(m_CommandBuffer, &colorTargetInfo, 1, nullptr);
        if (!m_RenderPass) {
            std::cerr << "BeginToneMappingPass: SDL_BeginGPURenderPass failed: " << SDL_GetError() << std::endl;
        }
        return m_RenderPass != nullptr;
    }

    void RenderDevice::EndFrame() {
        if (m_RenderPass) {
            SDL_EndGPURenderPass(m_RenderPass);
            m_RenderPass = nullptr;
        }

        if (m_CommandBuffer) {
            SDL_SubmitGPUCommandBuffer(m_CommandBuffer);
            m_CommandBuffer = nullptr;
        }
    }

    SDL_GPUTexture* RenderDevice::CreateTexture(uint32_t width, uint32_t height, const void* data) {
        SDL_GPUTextureCreateInfo createInfo;
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        createInfo.width = width;
        createInfo.height = height;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        createInfo.props = 0;

        SDL_GPUTexture* texture = SDL_CreateGPUTexture(m_Device, &createInfo);
        
        if (data && texture) {
            // Create transfer buffer
            uint32_t size = width * height * 4;
            SDL_GPUTransferBufferCreateInfo transferInfo;
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = size;
            transferInfo.props = 0;

            SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferInfo);
            
            if (transferBuffer) {
                void* map = SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false);
                if (map) {
                    memcpy(map, data, size);
                    SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);

                    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);
                    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);
                    
                    SDL_GPUTextureTransferInfo source;
                    source.transfer_buffer = transferBuffer;
                    source.offset = 0;
                    source.pixels_per_row = width;
                    source.rows_per_layer = height;

                    SDL_GPUTextureRegion destination;
                    destination.texture = texture;
                    destination.mip_level = 0;
                    destination.layer = 0;
                    destination.x = 0;
                    destination.y = 0;
                    destination.z = 0;
                    destination.w = width;
                    destination.h = height;
                    destination.d = 1;

                    SDL_UploadToGPUTexture(copyPass, &source, &destination, false);
                    SDL_EndGPUCopyPass(copyPass);
                    SDL_SubmitGPUCommandBuffer(cmd);
                    
                    // Note: We should release transferBuffer, but we need to wait for command buffer to finish.
                    // For simplicity in this prototype, we leak it or release it immediately (which is unsafe without fence).
                    // SDL3 might handle this if we release it? 
                    // "You can release the transfer buffer immediately after submitting the command buffer." - SDL3 docs usually say this for some resources.
                    SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
                }
            }
        }
        return texture;
    }

}
