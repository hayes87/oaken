#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Shader.h"
#include "Skeleton.h"
#include "Animation.h"
#include "../Platform/RenderDevice.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>

namespace Resources {

    ResourceManager::ResourceManager() {
    }

    ResourceManager::~ResourceManager() {
        Shutdown();
    }

    void ResourceManager::Init(Platform::RenderDevice* renderDevice) {
        m_RenderDevice = renderDevice;
        std::cout << "[ResourceManager] Initialized" << std::endl;
    }

    void ResourceManager::Shutdown() {
        m_Resources.clear();
        std::cout << "[ResourceManager] Shutdown" << std::endl;
    }

    void ResourceManager::Update() {
        static int frameCount = 0;
        frameCount++;
        
        for (auto& [path, resource] : m_Resources) {
            std::error_code ec;
            if (std::filesystem::exists(path, ec)) {
                auto currentWriteTime = std::filesystem::last_write_time(path, ec);
                
                // Debug log every 600 frames (approx 10s)
                if (frameCount % 600 == 0) {
                    // std::cout << "[ResourceManager] Checking " << path << std::endl;
                }

                if (!ec && currentWriteTime > resource->GetLastWriteTime()) {
                    std::cout << "[ResourceManager] Detected change in " << path << ". Reloading..." << std::endl;
                    
                    // Generic Reload Logic
                    if (resource->Reload()) {
                         resource->SetLastWriteTime(currentWriteTime);
                         std::cout << "[ResourceManager] Reloaded " << path << std::endl;
                    } else {
                        // Fallback for now (Legacy Texture Reload) until all resources implement Reload()
                        auto texture = std::dynamic_pointer_cast<Texture>(resource);
                        if (texture) {
                            // ... (Existing Texture Reload Logic) ...
                            std::vector<char> data = ReadFile(path);
                            if (!data.empty()) {
                                struct OakTexHeader {
                                    char signature[4];
                                    uint32_t width;
                                    uint32_t height;
                                    uint32_t channels;
                                    uint32_t format;
                                };
                                if (data.size() >= sizeof(OakTexHeader)) {
                                    OakTexHeader* header = reinterpret_cast<OakTexHeader*>(data.data());
                                    if (strncmp(header->signature, "OAKT", 4) == 0) {
                                        const char* pixelData = data.data() + sizeof(OakTexHeader);
                                        SDL_GPUTexture* gpuTexture = m_RenderDevice->CreateTexture(header->width, header->height, pixelData);
                                        if (gpuTexture) {
                                            texture->UpdateTexture(gpuTexture, header->width, header->height);
                                            resource->SetLastWriteTime(currentWriteTime);
                                            std::cout << "[ResourceManager] Reloaded " << path << std::endl;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<char> ResourceManager::ReadFile(const std::string& path) {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file.is_open()) {
            std::cerr << "[ResourceManager] Failed to open file: " << path << std::endl;
            return {};
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close(); // Explicitly close, though destructor would do it too.

        return buffer;
    }

    std::shared_ptr<Skeleton> ResourceManager::LoadSkeleton(const std::string& path) {
        if (m_Resources.find(path) != m_Resources.end()) {
            return std::dynamic_pointer_cast<Skeleton>(m_Resources[path]);
        }

        auto skeleton = std::make_shared<Skeleton>();
        skeleton->m_Path = path;
        if (skeleton->Load(path)) {
            std::error_code ec;
            skeleton->SetLastWriteTime(std::filesystem::last_write_time(path, ec));
            m_Resources[path] = skeleton;
            return skeleton;
        }
        return nullptr;
    }

    std::shared_ptr<Animation> ResourceManager::LoadAnimation(const std::string& path) {
        if (m_Resources.find(path) != m_Resources.end()) {
            return std::dynamic_pointer_cast<Animation>(m_Resources[path]);
        }

        auto animation = std::make_shared<Animation>();
        animation->m_Path = path;
        if (animation->Load(path)) {
            std::error_code ec;
            animation->SetLastWriteTime(std::filesystem::last_write_time(path, ec));
            m_Resources[path] = animation;
            return animation;
        }
        return nullptr;
    }

    std::shared_ptr<Texture> ResourceManager::LoadTexture(const std::string& path) {
        // Check Cache
        if (m_Resources.find(path) != m_Resources.end()) {
            return std::dynamic_pointer_cast<Texture>(m_Resources[path]);
        }

        std::vector<char> data = ReadFile(path);
        if (data.empty()) return nullptr;

        // Parse Header
        struct OakTexHeader {
            char signature[4];
            uint32_t width;
            uint32_t height;
            uint32_t channels;
            uint32_t format;
        };

        if (data.size() < sizeof(OakTexHeader)) return nullptr;

        OakTexHeader* header = reinterpret_cast<OakTexHeader*>(data.data());
        if (strncmp(header->signature, "OAKT", 4) != 0) {
            std::cerr << "[ResourceManager] Invalid texture signature: " << path << std::endl;
            return nullptr;
        }

        const char* pixelData = data.data() + sizeof(OakTexHeader);
        
        // Create GPU Texture
        SDL_GPUTexture* gpuTexture = m_RenderDevice->CreateTexture(header->width, header->height, pixelData);
        if (!gpuTexture) return nullptr;

        auto texture = std::make_shared<Texture>(m_RenderDevice->GetDevice(), header->width, header->height, gpuTexture);
        texture->m_Path = path;
        texture->m_LastWriteTime = std::filesystem::last_write_time(path);
        
        m_Resources[path] = texture;
        return texture;
    }

    std::shared_ptr<Mesh> ResourceManager::LoadMesh(const std::string& path) {
        // Check Cache
        if (m_Resources.find(path) != m_Resources.end()) {
            return std::dynamic_pointer_cast<Mesh>(m_Resources[path]);
        }

        auto mesh = std::make_shared<Mesh>(m_RenderDevice->GetDevice(), nullptr, nullptr, 0, 0);
        mesh->m_Path = path;
        
        if (mesh->Reload()) {
            mesh->m_LastWriteTime = std::filesystem::last_write_time(path);
            m_Resources[path] = mesh;
            return mesh;
        }
        
        return nullptr;
    }

    std::shared_ptr<Mesh> ResourceManager::CreatePrimitiveMesh(const std::string& name, 
        const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
        
        // Check cache first
        std::string cacheKey = "primitive:" + name;
        if (m_Resources.find(cacheKey) != m_Resources.end()) {
            return std::dynamic_pointer_cast<Mesh>(m_Resources[cacheKey]);
        }

        SDL_GPUDevice* device = m_RenderDevice->GetDevice();

        uint32_t vertexDataSize = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
        uint32_t indexDataSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

        // Create GPU Buffers
        SDL_GPUBufferCreateInfo vertexBufferInfo = {};
        vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vertexBufferInfo.size = vertexDataSize;
        SDL_GPUBuffer* vertexBuffer = SDL_CreateGPUBuffer(device, &vertexBufferInfo);

        SDL_GPUBufferCreateInfo indexBufferInfo = {};
        indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        indexBufferInfo.size = indexDataSize;
        SDL_GPUBuffer* indexBuffer = SDL_CreateGPUBuffer(device, &indexBufferInfo);

        if (!vertexBuffer || !indexBuffer) {
            if (vertexBuffer) SDL_ReleaseGPUBuffer(device, vertexBuffer);
            if (indexBuffer) SDL_ReleaseGPUBuffer(device, indexBuffer);
            return nullptr;
        }

        // Upload data
        uint32_t totalSize = vertexDataSize + indexDataSize;
        SDL_GPUTransferBufferCreateInfo transferInfo = {};
        transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transferInfo.size = totalSize;

        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
        if (!transferBuffer) {
            SDL_ReleaseGPUBuffer(device, vertexBuffer);
            SDL_ReleaseGPUBuffer(device, indexBuffer);
            return nullptr;
        }

        void* map = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
        if (map) {
            SDL_memcpy(map, vertices.data(), vertexDataSize);
            SDL_memcpy(static_cast<char*>(map) + vertexDataSize, indices.data(), indexDataSize);
            SDL_UnmapGPUTransferBuffer(device, transferBuffer);

            SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device);
            SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

            SDL_GPUTransferBufferLocation source = {};
            source.transfer_buffer = transferBuffer;
            source.offset = 0;

            SDL_GPUBufferRegion destination = {};
            destination.buffer = vertexBuffer;
            destination.size = vertexDataSize;
            destination.offset = 0;

            SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

            source.offset = vertexDataSize;
            destination.buffer = indexBuffer;
            destination.size = indexDataSize;

            SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

            SDL_EndGPUCopyPass(copyPass);
            SDL_SubmitGPUCommandBuffer(cmd);
        }
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);

        auto mesh = std::make_shared<Mesh>(device, vertexBuffer, indexBuffer, 
            static_cast<uint32_t>(vertices.size()), static_cast<uint32_t>(indices.size()));
        mesh->m_Path = cacheKey;
        m_Resources[cacheKey] = mesh;

        std::cout << "[ResourceManager] Created primitive mesh: " << name 
                  << " (" << vertices.size() << " verts, " << indices.size() << " indices)" << std::endl;
        
        return mesh;
    }

    std::shared_ptr<Shader> ResourceManager::LoadShader(const std::string& path, SDL_GPUShaderStage stage,
        uint32_t samplers, uint32_t storageTextures, uint32_t storageBuffers, uint32_t uniformBuffers) {
        
        // Debug Log
        std::cout << "[ResourceManager] LoadShader: " << path 
                  << " | SB: " << storageBuffers 
                  << " | UB: " << uniformBuffers << std::endl;

        // Check Cache
        if (m_Resources.find(path) != m_Resources.end()) {
            return std::dynamic_pointer_cast<Shader>(m_Resources[path]);
        }

        auto shader = std::make_shared<Shader>(m_RenderDevice->GetDevice(), nullptr, stage, samplers, storageTextures, storageBuffers, uniformBuffers);
        shader->m_Path = path;

        if (shader->Reload()) {
            shader->m_LastWriteTime = std::filesystem::last_write_time(path);
            m_Resources[path] = shader;
            return shader;
        }

        return nullptr;
    }

}
