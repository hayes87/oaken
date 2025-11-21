#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <filesystem>
#include <SDL3/SDL.h>

namespace Platform { class RenderDevice; }

namespace Resources {

    class Texture;
    class Mesh;
    class Shader;
    class Skeleton;
    class Animation;

    // Base class for all resources (Mesh, Texture, Shader, etc.)
    class Resource {
    public:
        virtual ~Resource() = default;
        
        // Returns true if reload was successful
        virtual bool Reload() { return false; }

        const std::string& GetPath() const { return m_Path; }
        std::filesystem::file_time_type GetLastWriteTime() const { return m_LastWriteTime; }
        void SetLastWriteTime(std::filesystem::file_time_type time) { m_LastWriteTime = time; }
        
    protected:
        std::string m_Path;
        std::filesystem::file_time_type m_LastWriteTime;
        friend class ResourceManager;
    };

    class ResourceManager {
    public:
        ResourceManager();
        ~ResourceManager();

        void Init(Platform::RenderDevice* renderDevice);
        void Shutdown();
        void Update(); // Check for hot reloads

        std::shared_ptr<Texture> LoadTexture(const std::string& path);
        std::shared_ptr<Mesh> LoadMesh(const std::string& path);
        std::shared_ptr<Shader> LoadShader(const std::string& path, SDL_GPUShaderStage stage, 
            uint32_t samplers = 0, uint32_t storageTextures = 0, uint32_t storageBuffers = 0, uint32_t uniformBuffers = 0);
        std::shared_ptr<Skeleton> LoadSkeleton(const std::string& path);
        std::shared_ptr<Animation> LoadAnimation(const std::string& path);

        static std::vector<char> ReadFile(const std::string& path);

        // Placeholder for future generic load function
        // static std::shared_ptr<Resource> LoadResource(const std::string& path);

    private:
        Platform::RenderDevice* m_RenderDevice = nullptr;
        // Cache of loaded resources
        std::unordered_map<std::string, std::shared_ptr<Resource>> m_Resources;
    };

}
