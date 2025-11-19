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

        // Request DXIL (D3D12) or SPIR-V (Vulkan)
        m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
        
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

    void RenderDevice::Shutdown() {
        if (m_Device) {
            if (m_Window) {
                SDL_ReleaseWindowFromGPUDevice(m_Device, m_Window->GetNativeWindow());
            }
            SDL_DestroyGPUDevice(m_Device);
            m_Device = nullptr;
        }
    }

    void RenderDevice::BeginFrame() {
        m_CommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
        if (!m_CommandBuffer) {
            return;
        }

        if (!SDL_AcquireGPUSwapchainTexture(m_CommandBuffer, m_Window->GetNativeWindow(), &m_SwapchainTexture, nullptr, nullptr)) {
            return;
        }

        if (m_SwapchainTexture) {
            SDL_GPUColorTargetInfo colorTargetInfo = {};
            colorTargetInfo.texture = m_SwapchainTexture;
            colorTargetInfo.clear_color = { 0.39f, 0.58f, 0.93f, 1.0f }; // Cornflower Blue
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

            m_RenderPass = SDL_BeginGPURenderPass(m_CommandBuffer, &colorTargetInfo, 1, nullptr);
        }
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

}
