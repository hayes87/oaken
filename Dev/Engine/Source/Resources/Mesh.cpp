#include "Mesh.h"
#include "../Platform/RenderDevice.h"
#include <set>

namespace Resources {

    Mesh::Mesh(SDL_GPUDevice* device, SDL_GPUBuffer* vertexBuffer, SDL_GPUBuffer* indexBuffer, uint32_t vertexCount, uint32_t indexCount)
        : m_Device(device), m_VertexBuffer(vertexBuffer), m_IndexBuffer(indexBuffer), m_VertexCount(vertexCount), m_IndexCount(indexCount)
    {
    }

    Mesh::~Mesh() {
        if (m_VertexBuffer) SDL_ReleaseGPUBuffer(m_Device, m_VertexBuffer);
        if (m_IndexBuffer) SDL_ReleaseGPUBuffer(m_Device, m_IndexBuffer);
    }

    void Mesh::UpdateMesh(SDL_GPUBuffer* vertexBuffer, SDL_GPUBuffer* indexBuffer, uint32_t vertexCount, uint32_t indexCount) {
        if (m_VertexBuffer) SDL_ReleaseGPUBuffer(m_Device, m_VertexBuffer);
        if (m_IndexBuffer) SDL_ReleaseGPUBuffer(m_Device, m_IndexBuffer);

        m_VertexBuffer = vertexBuffer;
        m_IndexBuffer = indexBuffer;
        m_VertexCount = vertexCount;
        m_IndexCount = indexCount;
    }

    bool Mesh::Reload() {
        std::vector<char> data = ResourceManager::ReadFile(m_Path);
        if (data.empty()) return false;

        // New header with jointRemapCount
        struct OakMeshHeader {
            char signature[4];
            uint32_t vertexCount;
            uint32_t indexCount;
            uint32_t boneCount;       // COMPACT bone count
            uint32_t jointRemapCount; // Same as boneCount (for joint_remaps)
        };

        if (data.size() < sizeof(OakMeshHeader)) return false;

        OakMeshHeader* header = reinterpret_cast<OakMeshHeader*>(data.data());
        if (strncmp(header->signature, "OAKM", 4) != 0) return false;

        uint32_t vertexDataSize = header->vertexCount * sizeof(Vertex);
        uint32_t indexDataSize = header->indexCount * sizeof(uint32_t);
        uint32_t ibmDataSize = header->boneCount * sizeof(glm::mat4);
        uint32_t remapDataSize = header->jointRemapCount * sizeof(uint16_t);
        
        const char* vertexData = data.data() + sizeof(OakMeshHeader);
        const char* indexData = vertexData + vertexDataSize;
        const char* ibmData = indexData + indexDataSize;
        const char* remapData = ibmData + ibmDataSize;

        // Read COMPACT IBMs
        m_InverseBindMatrices.clear();
        if (header->boneCount > 0) {
            m_InverseBindMatrices.resize(header->boneCount);
            memcpy(m_InverseBindMatrices.data(), ibmData, ibmDataSize);
        }
        
        // Read joint_remaps
        m_JointRemaps.clear();
        if (header->jointRemapCount > 0) {
            m_JointRemaps.resize(header->jointRemapCount);
            memcpy(m_JointRemaps.data(), remapData, remapDataSize);
        }

        // Create GPU Buffers
        SDL_GPUBufferCreateInfo vertexBufferInfo = {};
        vertexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        vertexBufferInfo.size = vertexDataSize;
        SDL_GPUBuffer* newVertexBuffer = SDL_CreateGPUBuffer(m_Device, &vertexBufferInfo);

        SDL_GPUBufferCreateInfo indexBufferInfo = {};
        indexBufferInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
        indexBufferInfo.size = indexDataSize;
        SDL_GPUBuffer* newIndexBuffer = SDL_CreateGPUBuffer(m_Device, &indexBufferInfo);

        if (newVertexBuffer && newIndexBuffer) {
            // Upload
            uint32_t totalSize = vertexDataSize + indexDataSize;
            SDL_GPUTransferBufferCreateInfo transferInfo = {};
            transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            transferInfo.size = totalSize;

            SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferInfo);
            if (transferBuffer) {
                void* map = SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false);
                if (map) {
                    SDL_memcpy(map, vertexData, vertexDataSize);
                    SDL_memcpy(static_cast<char*>(map) + vertexDataSize, indexData, indexDataSize);
                    SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);

                    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);
                    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

                    SDL_GPUTransferBufferLocation source = {};
                    source.transfer_buffer = transferBuffer;
                    source.offset = 0;

                    SDL_GPUBufferRegion destination = {};
                    destination.buffer = newVertexBuffer;
                    destination.size = vertexDataSize;
                    destination.offset = 0;

                    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

                    source.offset = vertexDataSize;
                    destination.buffer = newIndexBuffer;
                    destination.size = indexDataSize;

                    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

                    SDL_EndGPUCopyPass(copyPass);
                    SDL_SubmitGPUCommandBuffer(cmd);
                    SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);

                    UpdateMesh(newVertexBuffer, newIndexBuffer, header->vertexCount, header->indexCount);
                    return true;
                }
                SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
            }
        }

        if (newVertexBuffer) SDL_ReleaseGPUBuffer(m_Device, newVertexBuffer);
        if (newIndexBuffer) SDL_ReleaseGPUBuffer(m_Device, newIndexBuffer);

        return false; 
    }

}
