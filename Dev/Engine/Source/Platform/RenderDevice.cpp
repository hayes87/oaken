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

        // Always create shadow map (needed for shader binding even if shadows disabled)
        CreateShadowMapTexture(m_ShadowMapSize);

        return true;
    }

    SDL_GPUTextureFormat RenderDevice::GetSwapchainTextureFormat() const {
        return SDL_GetGPUSwapchainTextureFormat(m_Device, m_Window->GetNativeWindow());
    }

    void RenderDevice::Shutdown() {
        if (m_Device) {
            if (m_ShadowSampler) {
                SDL_ReleaseGPUSampler(m_Device, m_ShadowSampler);
                m_ShadowSampler = nullptr;
            }
            if (m_ShadowMapTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_ShadowMapTexture);
                m_ShadowMapTexture = nullptr;
            }
            if (m_DepthSampler) {
                SDL_ReleaseGPUSampler(m_Device, m_DepthSampler);
                m_DepthSampler = nullptr;
            }
            if (m_TileLightIndicesBuffer) {
                SDL_ReleaseGPUBuffer(m_Device, m_TileLightIndicesBuffer);
                m_TileLightIndicesBuffer = nullptr;
            }
            if (m_LightBuffer) {
                SDL_ReleaseGPUBuffer(m_Device, m_LightBuffer);
                m_LightBuffer = nullptr;
            }
            if (m_BloomBrightTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_BloomBrightTexture);
                m_BloomBrightTexture = nullptr;
            }
            if (m_BloomBlurTextureA) {
                SDL_ReleaseGPUTexture(m_Device, m_BloomBlurTextureA);
                m_BloomBlurTextureA = nullptr;
            }
            if (m_BloomBlurTextureB) {
                SDL_ReleaseGPUTexture(m_Device, m_BloomBlurTextureB);
                m_BloomBlurTextureB = nullptr;
            }
            if (m_HDRTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_HDRTexture);
                m_HDRTexture = nullptr;
            }
            if (m_DepthTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_DepthTexture);
                m_DepthTexture = nullptr;
            }
            // Release SSGI textures
            if (m_SSGITexture) {
                SDL_ReleaseGPUTexture(m_Device, m_SSGITexture);
                m_SSGITexture = nullptr;
            }
            if (m_SSGIHistoryTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_SSGIHistoryTexture);
                m_SSGIHistoryTexture = nullptr;
            }
            if (m_SSGIDenoiseTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_SSGIDenoiseTexture);
                m_SSGIDenoiseTexture = nullptr;
            }
            if (m_NoiseTexture) {
                SDL_ReleaseGPUTexture(m_Device, m_NoiseTexture);
                m_NoiseTexture = nullptr;
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
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;  // For Forward+ depth read
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

    void RenderDevice::CreateBloomTextures(uint32_t width, uint32_t height) {
        // Bloom textures at half resolution for performance
        uint32_t bloomWidth = width / 2;
        uint32_t bloomHeight = height / 2;
        
        // Release existing textures
        if (m_BloomBrightTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_BloomBrightTexture);
        }
        if (m_BloomBlurTextureA) {
            SDL_ReleaseGPUTexture(m_Device, m_BloomBlurTextureA);
        }
        if (m_BloomBlurTextureB) {
            SDL_ReleaseGPUTexture(m_Device, m_BloomBlurTextureB);
        }
        
        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format for bloom
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        createInfo.width = bloomWidth;
        createInfo.height = bloomHeight;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        
        m_BloomBrightTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        m_BloomBlurTextureA = SDL_CreateGPUTexture(m_Device, &createInfo);
        m_BloomBlurTextureB = SDL_CreateGPUTexture(m_Device, &createInfo);
        
        m_BloomWidth = bloomWidth;
        m_BloomHeight = bloomHeight;
        
        if (m_BloomBrightTexture && m_BloomBlurTextureA && m_BloomBlurTextureB) {
            std::cout << "Bloom textures created: " << bloomWidth << "x" << bloomHeight << " (RGBA16F)" << std::endl;
        }
    }

    void RenderDevice::CreateSSGITextures(uint32_t width, uint32_t height) {
        // SSGI at half resolution for performance
        uint32_t ssgiWidth = width / 2;
        uint32_t ssgiHeight = height / 2;
        
        // Release existing textures
        if (m_SSGITexture) {
            SDL_ReleaseGPUTexture(m_Device, m_SSGITexture);
        }
        if (m_SSGIHistoryTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_SSGIHistoryTexture);
        }
        if (m_SSGIDenoiseTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_SSGIDenoiseTexture);
        }
        
        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  // HDR format for GI
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        createInfo.width = ssgiWidth;
        createInfo.height = ssgiHeight;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        
        m_SSGITexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        m_SSGIHistoryTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        m_SSGIDenoiseTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        
        m_SSGIWidth = ssgiWidth;
        m_SSGIHeight = ssgiHeight;
        m_SSGIWasReset = true;  // Signal that history buffer is invalid
        
        if (m_SSGITexture && m_SSGIHistoryTexture && m_SSGIDenoiseTexture) {
            std::cout << "SSGI textures created: " << ssgiWidth << "x" << ssgiHeight << " (RGBA16F)" << std::endl;
        }
    }

    void RenderDevice::CreateNoiseTexture() {
        // Create 64x64 blue noise texture for ray jittering
        const uint32_t noiseSize = 64;
        
        if (m_NoiseTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_NoiseTexture);
        }
        
        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;  // RG for 2D noise
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        createInfo.width = noiseSize;
        createInfo.height = noiseSize;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        
        m_NoiseTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        
        if (m_NoiseTexture) {
            // Generate blue noise pattern (using interleaved gradient noise)
            std::vector<uint8_t> noiseData(noiseSize * noiseSize * 2);
            
            for (uint32_t y = 0; y < noiseSize; y++) {
                for (uint32_t x = 0; x < noiseSize; x++) {
                    // Interleaved gradient noise (R)
                    float n1 = fmodf(52.9829189f * fmodf(0.06711056f * float(x) + 0.00583715f * float(y), 1.0f), 1.0f);
                    // Different offset for second channel (G)
                    float n2 = fmodf(52.9829189f * fmodf(0.00583715f * float(x) + 0.06711056f * float(y), 1.0f), 1.0f);
                    
                    size_t idx = (y * noiseSize + x) * 2;
                    noiseData[idx + 0] = static_cast<uint8_t>(n1 * 255.0f);
                    noiseData[idx + 1] = static_cast<uint8_t>(n2 * 255.0f);
                }
            }
            
            // Upload noise data to GPU
            SDL_GPUTransferBufferCreateInfo transferInfo = {};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = noiseData.size();
            
            SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferInfo);
            if (transferBuffer) {
                void* map = SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false);
                memcpy(map, noiseData.data(), noiseData.size());
                SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);
                
                SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(m_Device);
                SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);
                
                SDL_GPUTextureTransferInfo srcInfo = {};
                srcInfo.transfer_buffer = transferBuffer;
                srcInfo.offset = 0;
                
                SDL_GPUTextureRegion dstRegion = {};
                dstRegion.texture = m_NoiseTexture;
                dstRegion.w = noiseSize;
                dstRegion.h = noiseSize;
                dstRegion.d = 1;
                
                SDL_UploadToGPUTexture(copyPass, &srcInfo, &dstRegion, false);
                
                SDL_EndGPUCopyPass(copyPass);
                SDL_SubmitGPUCommandBuffer(uploadCmd);
                
                SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
            }
            
            std::cout << "SSGI noise texture created: " << noiseSize << "x" << noiseSize << std::endl;
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
        
        // Recreate Bloom textures if size changed or don't exist (only if bloom enabled)
        uint32_t bloomWidth = w / 2;
        uint32_t bloomHeight = h / 2;
        if (m_BloomEnabled && (!m_BloomBrightTexture || bloomWidth != m_BloomWidth || bloomHeight != m_BloomHeight)) {
            CreateBloomTextures(w, h);
        }
        
        // Recreate SSGI textures if size changed or don't exist (only if SSGI enabled)
        uint32_t ssgiWidth = w / 2;
        uint32_t ssgiHeight = h / 2;
        if (m_SSGIEnabled && (!m_SSGITexture || ssgiWidth != m_SSGIWidth || ssgiHeight != m_SSGIHeight)) {
            CreateSSGITextures(w, h);
        }
        
        // Create noise texture if it doesn't exist (only needed once)
        if (m_SSGIEnabled && !m_NoiseTexture) {
            CreateNoiseTexture();
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

    void RenderDevice::CreateForwardPlusBuffers(uint32_t width, uint32_t height) {
        // Calculate number of tiles
        m_NumTilesX = (width + FORWARD_PLUS_TILE_SIZE - 1) / FORWARD_PLUS_TILE_SIZE;
        m_NumTilesY = (height + FORWARD_PLUS_TILE_SIZE - 1) / FORWARD_PLUS_TILE_SIZE;
        
        // Size of tile light indices buffer:
        // Each tile has: [count] + [MAX_LIGHTS_PER_TILE indices]
        uint32_t newTileBufferSize = m_NumTilesX * m_NumTilesY * (MAX_LIGHTS_PER_TILE + 1) * sizeof(uint32_t);
        
        // Only recreate if size changed
        if (newTileBufferSize != m_TileBufferSize) {
            if (m_TileLightIndicesBuffer) {
                SDL_ReleaseGPUBuffer(m_Device, m_TileLightIndicesBuffer);
            }
            
            SDL_GPUBufferCreateInfo bufferInfo = {};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
            bufferInfo.size = newTileBufferSize;
            
            m_TileLightIndicesBuffer = SDL_CreateGPUBuffer(m_Device, &bufferInfo);
            m_TileBufferSize = newTileBufferSize;
            
            if (m_TileLightIndicesBuffer) {
                std::cout << "Forward+ tile buffer created: " << m_NumTilesX << "x" << m_NumTilesY 
                          << " tiles (" << newTileBufferSize / 1024 << " KB)" << std::endl;
            }
        }
        
        // Create light buffer if it doesn't exist
        if (!m_LightBuffer) {
            SDL_GPUBufferCreateInfo bufferInfo = {};
            bufferInfo.usage = SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
            // Light data: numLights (4 bytes) + padding (12 bytes) + MAX_POINT_LIGHTS * 32 bytes (2 vec4s per light)
            bufferInfo.size = 16 + MAX_POINT_LIGHTS * 32;
            
            m_LightBuffer = SDL_CreateGPUBuffer(m_Device, &bufferInfo);
            if (m_LightBuffer) {
                std::cout << "Forward+ light buffer created: " << bufferInfo.size / 1024 << " KB" << std::endl;
            }
        }
        
        // Create depth sampler if it doesn't exist
        if (!m_DepthSampler) {
            SDL_GPUSamplerCreateInfo samplerInfo = {};
            samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
            samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
            samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
            m_DepthSampler = SDL_CreateGPUSampler(m_Device, &samplerInfo);
        }
    }

    bool RenderDevice::BeginDepthPrePass() {
        if (!m_FrameValid || !m_CommandBuffer || !m_SwapchainTexture) return false;
        
        // Depth-only pass - no color attachment
        SDL_GPUDepthStencilTargetInfo depthStencilInfo = {};
        depthStencilInfo.texture = m_DepthTexture;
        depthStencilInfo.clear_depth = 1.0f;
        depthStencilInfo.load_op = SDL_GPU_LOADOP_CLEAR;
        depthStencilInfo.store_op = SDL_GPU_STOREOP_STORE;
        depthStencilInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
        depthStencilInfo.stencil_store_op = SDL_GPU_STOREOP_STORE;
        depthStencilInfo.cycle = true;
        
        m_RenderPass = SDL_BeginGPURenderPass(m_CommandBuffer, nullptr, 0, &depthStencilInfo);
        return m_RenderPass != nullptr;
    }

    void RenderDevice::UpdateLightBuffer(const void* lightData, uint32_t numLights) {
        if (!m_LightBuffer || !m_CommandBuffer) return;
        
        // Calculate data size: header (16 bytes) + lights (32 bytes each)
        uint32_t dataSize = 16 + numLights * 32;
        
        // Create transfer buffer
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = dataSize;
        
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferInfo);
        if (!transferBuffer) return;
        
        void* map = SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false);
        if (map) {
            memcpy(map, lightData, dataSize);
            SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);
            
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(m_CommandBuffer);
            
            SDL_GPUTransferBufferLocation source = {};
            source.transfer_buffer = transferBuffer;
            source.offset = 0;
            
            SDL_GPUBufferRegion destination = {};
            destination.buffer = m_LightBuffer;
            destination.offset = 0;
            destination.size = dataSize;
            
            SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
            SDL_EndGPUCopyPass(copyPass);
        }
        
        SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
    }

    void RenderDevice::EnsureForwardPlusBuffers() {
        if (m_RenderWidth > 0 && m_RenderHeight > 0) {
            CreateForwardPlusBuffers(m_RenderWidth, m_RenderHeight);
        }
    }

    void RenderDevice::DispatchLightCulling(SDL_GPUComputePipeline* cullingPipeline, const glm::mat4& view, const glm::mat4& proj) {
        if (!m_CommandBuffer || !cullingPipeline || !m_ForwardPlusEnabled) return;
        if (!m_DepthTexture || !m_DepthSampler) return;  // Need depth texture for culling
        
        // Ensure Forward+ buffers exist
        CreateForwardPlusBuffers(m_RenderWidth, m_RenderHeight);
        if (!m_TileLightIndicesBuffer || !m_LightBuffer) return;
        
        // Begin compute pass - bind the tile buffer as writeable
        SDL_GPUStorageBufferReadWriteBinding tileBufferBinding = {};
        tileBufferBinding.buffer = m_TileLightIndicesBuffer;
        tileBufferBinding.cycle = false;
        
        SDL_GPUComputePass* computePass = SDL_BeginGPUComputePass(
            m_CommandBuffer,
            nullptr, 0,  // No storage texture bindings
            &tileBufferBinding, 1  // Tile buffer binding
        );
        
        if (!computePass) return;
        
        SDL_BindGPUComputePipeline(computePass, cullingPipeline);
        
        // Bind depth texture sampler (set 0, binding 0 in compute shader)
        SDL_GPUTextureSamplerBinding depthBinding = {};
        depthBinding.texture = m_DepthTexture;
        depthBinding.sampler = m_DepthSampler;
        SDL_BindGPUComputeSamplers(computePass, 0, &depthBinding, 1);
        
        // Bind light buffer (read-only storage at set 0, binding 1)
        SDL_GPUBuffer* storageBuffers[] = { m_LightBuffer };
        SDL_BindGPUComputeStorageBuffers(computePass, 0, storageBuffers, 1);
        
        // Push view data uniform
        struct ViewData {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 invProj;
            glm::vec4 screenSize;
        } viewData;
        
        viewData.view = view;
        viewData.proj = proj;
        viewData.invProj = glm::inverse(proj);
        viewData.screenSize = glm::vec4(
            static_cast<float>(m_RenderWidth),
            static_cast<float>(m_RenderHeight),
            1.0f / static_cast<float>(m_RenderWidth),
            1.0f / static_cast<float>(m_RenderHeight)
        );
        
        SDL_PushGPUComputeUniformData(m_CommandBuffer, 0, &viewData, sizeof(viewData));
        
        // Debug: Log tile calculation once
        static bool loggedTileCalc = false;
        if (!loggedTileCalc) {
            uint32_t computedNumTilesX = (m_RenderWidth + FORWARD_PLUS_TILE_SIZE - 1) / FORWARD_PLUS_TILE_SIZE;
            std::cout << "[Forward+] screenSize: " << m_RenderWidth << "x" << m_RenderHeight 
                      << ", numTilesX: " << m_NumTilesX << " (computed: " << computedNumTilesX << ")"
                      << ", TILE_SIZE: " << FORWARD_PLUS_TILE_SIZE << std::endl;
            loggedTileCalc = true;
        }
        
        // Dispatch compute shader - one workgroup per tile
        SDL_DispatchGPUCompute(computePass, m_NumTilesX, m_NumTilesY, 1);
        
        SDL_EndGPUComputePass(computePass);
    }

    void RenderDevice::CreateShadowMapTexture(uint32_t size) {
        if (m_ShadowMapTexture) {
            SDL_ReleaseGPUTexture(m_Device, m_ShadowMapTexture);
            m_ShadowMapTexture = nullptr;
        }
        if (m_ShadowSampler) {
            SDL_ReleaseGPUSampler(m_Device, m_ShadowSampler);
            m_ShadowSampler = nullptr;
        }
        
        // Create depth texture for shadow map
        SDL_GPUTextureCreateInfo createInfo = {};
        createInfo.type = SDL_GPU_TEXTURETYPE_2D;
        createInfo.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;  // Higher precision for shadows
        createInfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        createInfo.width = size;
        createInfo.height = size;
        createInfo.layer_count_or_depth = 1;
        createInfo.num_levels = 1;
        createInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
        
        m_ShadowMapTexture = SDL_CreateGPUTexture(m_Device, &createInfo);
        if (!m_ShadowMapTexture) {
            std::cerr << "Failed to create shadow map texture: " << SDL_GetError() << std::endl;
            return;
        }
        
        // Create comparison sampler for PCF shadow filtering
        // LESS_OR_EQUAL: returns 1.0 when reference <= stored (fragment is closer/same = lit)
        SDL_GPUSamplerCreateInfo samplerInfo = {};
        samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        samplerInfo.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        samplerInfo.enable_compare = true;
        
        m_ShadowSampler = SDL_CreateGPUSampler(m_Device, &samplerInfo);
        if (!m_ShadowSampler) {
            std::cerr << "Failed to create shadow sampler: " << SDL_GetError() << std::endl;
        }
        
        m_ShadowMapSize = size;
        std::cout << "Shadow map created: " << size << "x" << size << " (D32F)" << std::endl;
    }
    
    void RenderDevice::SetShadowMapSize(uint32_t size) {
        if (size != m_ShadowMapSize && size > 0) {
            m_ShadowMapSize = size;
            if (m_ShadowsEnabled && m_Device) {
                CreateShadowMapTexture(size);
            }
        }
    }
    
    bool RenderDevice::BeginShadowPass() {
        if (!m_FrameValid || !m_CommandBuffer) {
            return false;
        }
        
        // Create shadow map if it doesn't exist
        if (!m_ShadowMapTexture && m_ShadowsEnabled) {
            CreateShadowMapTexture(m_ShadowMapSize);
        }
        
        if (!m_ShadowMapTexture) {
            return false;
        }
        
        // Depth-only render pass targeting shadow map
        SDL_GPUDepthStencilTargetInfo depthTarget = {};
        depthTarget.texture = m_ShadowMapTexture;
        depthTarget.clear_depth = 1.0f;
        depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        depthTarget.store_op = SDL_GPU_STOREOP_STORE;
        depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        
        m_RenderPass = SDL_BeginGPURenderPass(m_CommandBuffer, nullptr, 0, &depthTarget);
        if (!m_RenderPass) {
            std::cerr << "BeginShadowPass: SDL_BeginGPURenderPass failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        return true;
    }
    
    void RenderDevice::EndShadowPass() {
        if (m_RenderPass) {
            SDL_EndGPURenderPass(m_RenderPass);
            m_RenderPass = nullptr;
        }
    }

}
