#pragma once
#include "ResourceManager.h"
#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Resources {

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        glm::vec4 weights;
        glm::vec4 joints;  // COMPACT joint indices (0 to usedJointCount-1)
    };

    class Mesh : public Resource {
    public:
        Mesh(SDL_GPUDevice* device, SDL_GPUBuffer* vertexBuffer, SDL_GPUBuffer* indexBuffer, uint32_t vertexCount, uint32_t indexCount);
        ~Mesh();

        SDL_GPUBuffer* GetVertexBuffer() const { return m_VertexBuffer; }
        SDL_GPUBuffer* GetIndexBuffer() const { return m_IndexBuffer; }
        uint32_t GetVertexCount() const { return m_VertexCount; }
        uint32_t GetIndexCount() const { return m_IndexCount; }
        
        // COMPACT inverse bind matrices (one per used joint)
        const std::vector<glm::mat4>& GetInverseBindMatrices() const { return m_InverseBindMatrices; }
        
        // joint_remaps[compact_index] = skeleton_index
        const std::vector<uint16_t>& GetJointRemaps() const { return m_JointRemaps; }
        
        uint32_t GetUsedJointCount() const { return static_cast<uint32_t>(m_JointRemaps.size()); }

        void UpdateMesh(SDL_GPUBuffer* vertexBuffer, SDL_GPUBuffer* indexBuffer, uint32_t vertexCount, uint32_t indexCount);

        virtual bool Reload() override;

    private:
        SDL_GPUDevice* m_Device;
        SDL_GPUBuffer* m_VertexBuffer;
        SDL_GPUBuffer* m_IndexBuffer;
        uint32_t m_VertexCount;
        uint32_t m_IndexCount;
        std::vector<glm::mat4> m_InverseBindMatrices;  // COMPACT - size = usedJointCount
        std::vector<uint16_t> m_JointRemaps;           // COMPACT -> skeleton mapping
    };

}
