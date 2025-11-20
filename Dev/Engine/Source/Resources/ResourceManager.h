#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

namespace Platform { class RenderDevice; }

namespace Resources {

    class Texture;

    // Base class for all resources (Mesh, Texture, Shader, etc.)
    class Resource {
    public:
        virtual ~Resource() = default;
        
        const std::string& GetPath() const { return m_Path; }
        
    protected:
        std::string m_Path;
        friend class ResourceManager;
    };

    class ResourceManager {
    public:
        ResourceManager();
        ~ResourceManager();

        void Init(Platform::RenderDevice* renderDevice);
        void Shutdown();

        std::shared_ptr<Texture> LoadTexture(const std::string& path);

        static std::vector<char> ReadFile(const std::string& path);

        // Placeholder for future generic load function
        // static std::shared_ptr<Resource> LoadResource(const std::string& path);

    private:
        Platform::RenderDevice* m_RenderDevice = nullptr;
        // Cache of loaded resources
        std::unordered_map<std::string, std::shared_ptr<Resource>> m_Resources;
    };

}
