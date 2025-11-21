#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <fstream>
#include <iomanip> // For std::quoted
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include <unordered_map>

using json = nlohmann::json;

namespace fs = std::filesystem;

struct OakTexHeader {
    char signature[4] = {'O', 'A', 'K', 'T'};
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t format = 0; // 0 = RGBA8
};

struct OakMeshHeader {
    char signature[4] = {'O', 'A', 'K', 'M'};
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 weights;
    glm::uvec4 joints;
};

void RecurseSkeleton(const aiNode* node, ozz::animation::offline::RawSkeleton::Joint& joint) {
    joint.name = node->mName.C_Str();
    
    aiVector3D scaling, position;
    aiQuaternion rotation;
    node->mTransformation.Decompose(scaling, rotation, position);
    
    joint.transform.translation = ozz::math::Float3(position.x, position.y, position.z);
    joint.transform.rotation = ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
    joint.transform.scale = ozz::math::Float3(scaling.x, scaling.y, scaling.z);
    
    joint.children.resize(node->mNumChildren);
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        RecurseSkeleton(node->mChildren[i], joint.children[i]);
    }
}

bool CookSkeleton(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Skeleton: " << input << " -> " << output << std::endl;

    Assimp::Importer importer;
    // Preserve hierarchy is important for skeleton
    const aiScene* scene = importer.ReadFile(input.string(), 0);

    if (!scene || !scene->mRootNode) {
        std::cerr << "[Cooker] Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    RecurseSkeleton(scene->mRootNode, raw_skeleton.roots[0]);

    ozz::animation::offline::SkeletonBuilder builder;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder(raw_skeleton);

    if (!skeleton) {
        std::cerr << "[Cooker] Failed to build skeleton" << std::endl;
        return false;
    }

    fs::path tempOutput = output;
    tempOutput += ".tmp";
    
    {
        ozz::io::File file(tempOutput.string().c_str(), "wb");
        if (!file.opened()) {
             std::cerr << "[Cooker] Failed to open output file: " << tempOutput << std::endl;
             return false;
        }
        ozz::io::OArchive archive(&file);
        archive << *skeleton;
    }

    if (fs::exists(output)) {
        try { fs::remove(output); } catch(...) {}
    }
    fs::rename(tempOutput, output);

    return true;
}

bool CookAnimation(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Animation: " << input << " -> " << output << std::endl;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(input.string(), 0);

    if (!scene || !scene->mRootNode) {
        std::cerr << "[Cooker] Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    if (!scene->HasAnimations()) {
        std::cout << "[Cooker] No animations found in source." << std::endl;
        return true; // Not an error, just nothing to do
    }

    // For now, just take the first animation
    const aiAnimation* anim = scene->mAnimations[0];
    
    ozz::animation::offline::RawAnimation raw_animation;
    raw_animation.duration = (float)anim->mDuration / (float)anim->mTicksPerSecond;
    
    // Re-extract skeleton to get joint order
    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    RecurseSkeleton(scene->mRootNode, raw_skeleton.roots[0]);
    
    ozz::animation::offline::SkeletonBuilder builder;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder(raw_skeleton);
    
    if (!skeleton) return false;
    
    raw_animation.tracks.resize(skeleton->num_joints());
    
    // Map joint name to index
    std::unordered_map<std::string, int> joint_map;
    for (int i = 0; i < skeleton->num_joints(); ++i) {
        joint_map[skeleton->joint_names()[i]] = i;
    }
    
    for (unsigned int i = 0; i < anim->mNumChannels; ++i) {
        const aiNodeAnim* channel = anim->mChannels[i];
        std::string name = channel->mNodeName.C_Str();
        
        if (joint_map.find(name) == joint_map.end()) continue;
        
        int joint_index = joint_map[name];
        ozz::animation::offline::RawAnimation::JointTrack& track = raw_animation.tracks[joint_index];
        
        // Position keys
        for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
            const aiVectorKey& key = channel->mPositionKeys[k];
            ozz::animation::offline::RawAnimation::TranslationKey out_key;
            out_key.time = (float)key.mTime / (float)anim->mTicksPerSecond;
            out_key.value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
            track.translations.push_back(out_key);
        }
        
        // Rotation keys
        for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
            const aiQuatKey& key = channel->mRotationKeys[k];
            ozz::animation::offline::RawAnimation::RotationKey out_key;
            out_key.time = (float)key.mTime / (float)anim->mTicksPerSecond;
            out_key.value = ozz::math::Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
            track.rotations.push_back(out_key);
        }
        
        // Scale keys
        for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
            const aiVectorKey& key = channel->mScalingKeys[k];
            ozz::animation::offline::RawAnimation::ScaleKey out_key;
            out_key.time = (float)key.mTime / (float)anim->mTicksPerSecond;
            out_key.value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
            track.scales.push_back(out_key);
        }
    }
    
    ozz::animation::offline::AnimationBuilder anim_builder;
    ozz::unique_ptr<ozz::animation::Animation> animation = anim_builder(raw_animation);
    
    if (!animation) {
        std::cerr << "[Cooker] Failed to build animation" << std::endl;
        return false;
    }
    
    fs::path tempOutput = output;
    tempOutput += ".tmp";
    
    {
        ozz::io::File file(tempOutput.string().c_str(), "wb");
        if (!file.opened()) {
             std::cerr << "[Cooker] Failed to open output file: " << tempOutput << std::endl;
             return false;
        }
        ozz::io::OArchive archive(&file);
        archive << *animation;
    }

    if (fs::exists(output)) {
        try { fs::remove(output); } catch(...) {}
    }
    fs::rename(tempOutput, output);

    return true;
}

bool CookShader(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Shader: " << input << " -> " << output << std::endl;

    // Determine shader stage from extension
    std::string stage;
    std::string ext = input.extension().string();
    std::string profile;

    if (ext == ".vert") {
        stage = "vertex";
        profile = "vs_6_0";
    } else if (ext == ".frag") {
        stage = "fragment";
        profile = "ps_6_0";
    } else if (ext == ".comp") {
        stage = "compute";
        profile = "cs_6_0";
    } else {
        std::cerr << "[Cooker] Unknown shader extension: " << ext << std::endl;
        return false;
    }

    // Ensure output directory exists
    if (output.has_parent_path()) {
        fs::create_directories(output.parent_path());
    }

    std::stringstream cmd;
    
    // Check output extension to determine compiler
    std::string outExt = output.extension().string();
    if (outExt == ".dxil") {
        // Use DXC for DirectX 12
        // dxc -T <profile> -E main <input> -Fo <output>
        cmd << "dxc -T " << profile << " -E main \"" << input.string() << "\" -Fo \"" << output.string() << "\"";
    } else {
        // Use GLSLC for Vulkan (SPIR-V)
        cmd << "glslc -fshader-stage=" << stage << " \"" << input.string() << "\" -o \"" << output.string() << "\"";
    }

    std::cout << "[Cooker] Executing: " << cmd.str() << std::endl;
    int result = std::system(cmd.str().c_str());

    if (result != 0) {
        std::cerr << "[Cooker] Shader compilation failed with code: " << result << std::endl;
        return false;
    }

    return true;
}

bool CookTexture(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Texture: " << input << " -> " << output << std::endl;
    
    int width, height, channels;
    // Force 4 channels (RGBA) for simplicity
    unsigned char* data = stbi_load(input.string().c_str(), &width, &height, &channels, 4);
    
    if (!data) {
        std::cerr << "[Cooker] Failed to load image: " << input << std::endl;
        return false;
    }

    // Write to a temporary file first, then rename (atomic-ish replace)
    // This prevents the Engine from trying to read a half-written file
    fs::path tempOutput = output;
    tempOutput += ".tmp";

    try {
        std::ofstream outFile(tempOutput, std::ios::binary);
        if (!outFile) {
            std::cerr << "[Cooker] Failed to open output file: " << tempOutput << std::endl;
            stbi_image_free(data);
            return false;
        }

        OakTexHeader header;
        header.width = static_cast<uint32_t>(width);
        header.height = static_cast<uint32_t>(height);
        header.channels = 4;
        header.format = 0; // RGBA8

        outFile.write(reinterpret_cast<const char*>(&header), sizeof(OakTexHeader));
        outFile.write(reinterpret_cast<const char*>(data), width * height * 4);
        outFile.close();

        stbi_image_free(data);
        
        // Now rename temp to final
        // On Windows, we might need to remove the destination first if it exists
        if (fs::exists(output)) {
            try {
                fs::remove(output);
            } catch (const fs::filesystem_error& e) {
                std::cerr << "[Cooker] Failed to remove existing output file: " << output << " Error: " << e.what() << std::endl;
                // Try to continue anyway, maybe rename will work (replace existing)
            }
        }
        
        try {
            fs::rename(tempOutput, output);
        } catch (const fs::filesystem_error& e) {
             std::cerr << "[Cooker] Failed to rename temp file to output: " << e.what() << std::endl;
             // Cleanup temp
             if (fs::exists(tempOutput)) fs::remove(tempOutput);
             return false;
        }
        
        return true;
    } catch (std::exception& e) {
        std::cerr << "[Cooker] Error processing file: " << e.what() << std::endl;
        // Cleanup temp
        if (fs::exists(tempOutput)) fs::remove(tempOutput);
        if (data) stbi_image_free(data);
        return false;
    }
}

bool CookMesh(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Mesh: " << input << " -> " << output << std::endl;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(input.string(), 
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_JoinIdenticalVertices);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cerr << "[Cooker] Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    // Build skeleton to get joint indices
    ozz::animation::offline::RawSkeleton raw_skeleton;
    raw_skeleton.roots.resize(1);
    RecurseSkeleton(scene->mRootNode, raw_skeleton.roots[0]);
    
    ozz::animation::offline::SkeletonBuilder builder;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton = builder(raw_skeleton);
    
    std::unordered_map<std::string, int> joint_map;
    if (skeleton) {
        for (int i = 0; i < skeleton->num_joints(); ++i) {
            joint_map[skeleton->joint_names()[i]] = i;
        }
    }

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t indexOffset = 0;

    // Simple merge of all meshes (ignoring node hierarchy transforms for now)
    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[i];
        uint32_t currentMeshVertexOffset = vertices.size();
        
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            Vertex vertex;
            vertex.position = { mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z };
            
            if (mesh->HasNormals()) {
                vertex.normal = { mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z };
            } else {
                vertex.normal = { 0.0f, 0.0f, 0.0f };
            }

            if (mesh->mTextureCoords[0]) {
                vertex.uv = { mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y };
            } else {
                vertex.uv = { 0.0f, 0.0f };
            }
            
            vertex.weights = {0.0f, 0.0f, 0.0f, 0.0f};
            vertex.joints = {0, 0, 0, 0};

            vertices.push_back(vertex);
        }

        if (mesh->HasBones()) {
            std::vector<int> boneCounts(mesh->mNumVertices, 0);
            
            for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                aiBone* bone = mesh->mBones[b];
                std::string boneName = bone->mName.C_Str();
                
                if (joint_map.find(boneName) == joint_map.end()) continue;
                int jointIndex = joint_map[boneName];
                
                for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                    aiVertexWeight weight = bone->mWeights[w];
                    uint32_t vertexId = weight.mVertexId;
                    float weightVal = weight.mWeight;
                    
                    if (boneCounts[vertexId] < 4) {
                        vertices[currentMeshVertexOffset + vertexId].weights[boneCounts[vertexId]] = weightVal;
                        vertices[currentMeshVertexOffset + vertexId].joints[boneCounts[vertexId]] = jointIndex;
                        boneCounts[vertexId]++;
                    }
                }
            }
            
            // Normalize weights
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                glm::vec4& w = vertices[currentMeshVertexOffset + v].weights;
                float total = w.x + w.y + w.z + w.w;
                if (total > 0.0f) {
                    w /= total;
                }
            }
        }

        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            aiFace face = mesh->mFaces[f];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j] + indexOffset);
            }
        }
        
        indexOffset += mesh->mNumVertices;
    }

    // Write to file
    fs::path tempOutput = output;
    tempOutput += ".tmp";

    try {
        std::ofstream outFile(tempOutput, std::ios::binary);
        if (!outFile) {
            std::cerr << "[Cooker] Failed to open output file: " << tempOutput << std::endl;
            return false;
        }

        OakMeshHeader header;
        header.vertexCount = static_cast<uint32_t>(vertices.size());
        header.indexCount = static_cast<uint32_t>(indices.size());

        outFile.write(reinterpret_cast<const char*>(&header), sizeof(OakMeshHeader));
        outFile.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));
        outFile.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));
        outFile.close();

        if (fs::exists(output)) fs::remove(output);
        fs::rename(tempOutput, output);
        
        return true;
    } catch (std::exception& e) {
        std::cerr << "[Cooker] Error writing mesh: " << e.what() << std::endl;
        if (fs::exists(tempOutput)) fs::remove(tempOutput);
        return false;
    }
}

struct OakLevelHeader {
    char signature[4] = {'O', 'A', 'K', 'L'};
    uint32_t version = 1;
    uint32_t entityCount = 0;
};

struct LocalTransform {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

bool CookScene(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Scene: " << input << " -> " << output << std::endl;
    
    std::ifstream inFile(input);
    if (!inFile) {
        std::cerr << "[Cooker] Failed to open input file: " << input << std::endl;
        return false;
    }

    json root;
    try {
        inFile >> root;
    } catch (const json::parse_error& e) {
        std::cerr << "[Cooker] JSON Parse Error: " << e.what() << std::endl;
        return false;
    }

    auto entities = root["entities"];
    
    fs::path tempOutput = output;
    tempOutput += ".tmp";

    try {
        std::ofstream outFile(tempOutput, std::ios::binary);
        if (!outFile) return false;

        OakLevelHeader header;
        header.entityCount = static_cast<uint32_t>(entities.size());
        outFile.write(reinterpret_cast<const char*>(&header), sizeof(OakLevelHeader));

        for (const auto& entityJson : entities) {
            // Name
            std::string name = entityJson.value("name", "Entity");
            uint32_t nameLen = static_cast<uint32_t>(name.length());
            outFile.write(reinterpret_cast<const char*>(&nameLen), sizeof(uint32_t));
            outFile.write(name.c_str(), nameLen);

            // Transform
            bool hasTransform = entityJson.contains("transform");
            outFile.write(reinterpret_cast<const char*>(&hasTransform), sizeof(bool));
            if (hasTransform) {
                auto t = entityJson["transform"];
                LocalTransform transform;
                transform.position = {t["position"][0], t["position"][1], t["position"][2]};
                transform.rotation = {t["rotation"][0], t["rotation"][1], t["rotation"][2]};
                transform.scale = {t["scale"][0], t["scale"][1], t["scale"][2]};
                outFile.write(reinterpret_cast<const char*>(&transform), sizeof(LocalTransform));
            }

            // Sprite (Placeholder)
            bool hasSprite = false; 
            outFile.write(reinterpret_cast<const char*>(&hasSprite), sizeof(bool));

            // Mesh
            bool hasMesh = entityJson.contains("mesh");
            outFile.write(reinterpret_cast<const char*>(&hasMesh), sizeof(bool));
            if (hasMesh) {
                std::string path = entityJson["mesh"].value("path", "");
                uint32_t pathLen = static_cast<uint32_t>(path.length());
                outFile.write(reinterpret_cast<const char*>(&pathLen), sizeof(uint32_t));
                outFile.write(path.c_str(), pathLen);
            }
        }
        
        outFile.close();
        
        if (fs::exists(output)) {
            try { fs::remove(output); } catch(...) {}
        }
        fs::rename(tempOutput, output);
        
        return true;
    } catch (std::exception& e) {
        std::cerr << "[Cooker] Error: " << e.what() << std::endl;
        if (fs::exists(tempOutput)) fs::remove(tempOutput);
        return false;
    }
}

void ProcessCommand(const std::string& commandLine) {
    std::stringstream ss(commandLine);
    std::string command;
    ss >> command;

    if (command == "COOK") {
        std::string type, inputPath, outputPath;
        ss >> type;
        
        // Handle quoted paths
        auto readPath = [&](std::string& path) {
            char c;
            ss >> std::ws; // Skip whitespace
            if (ss.peek() == '"') {
                ss >> std::quoted(path);
            } else {
                ss >> path;
            }
        };

        readPath(inputPath);
        readPath(outputPath);

        bool success = false;
        if (type == "TEXTURE") {
            success = CookTexture(inputPath, outputPath);
        } else if (type == "MESH") {
            success = CookMesh(inputPath, outputPath);
        } else if (type == "SCENE") {
            success = CookScene(inputPath, outputPath);
        } else if (type == "SHADER") {
            success = CookShader(inputPath, outputPath);
        } else if (type == "SKELETON") {
            success = CookSkeleton(inputPath, outputPath);
        } else if (type == "ANIMATION") {
            success = CookAnimation(inputPath, outputPath);
        } else {
            std::cerr << "[Cooker] Unknown asset type: " << type << std::endl;
        }

        if (success) {
            if (fs::exists(outputPath)) {
                std::cout << "SUCCESS " << outputPath << std::endl;
            } else {
                std::cout << "SKIPPED " << inputPath << " (No output generated)" << std::endl;
            }
        } else {
            std::cout << "FAILURE " << inputPath << std::endl;
        }
    } else if (command == "PING") {
        std::cout << "PONG" << std::endl;
    } else if (command == "EXIT") {
        exit(0);
    } else {
        std::cerr << "[Cooker] Unknown command: " << command << std::endl;
    }
}

int main(int argc, char** argv) {
    // Check for command line arguments (Batch Mode / Single Command)
    if (argc > 1) {
        // Expected usage: AssetCooker.exe COOK TEXTURE input output
        std::string command = argv[1];
        if (command == "COOK" && argc >= 5) {
            std::string type = argv[2];
            std::string input = argv[3];
            std::string output = argv[4];
            
            bool success = false;
            if (type == "TEXTURE") {
                success = CookTexture(input, output);
            } else if (type == "MESH") {
                success = CookMesh(input, output);
            } else if (type == "SCENE") {
                success = CookScene(input, output);
            } else if (type == "SHADER") {
                success = CookShader(input, output);
            } else if (type == "SKELETON") {
                success = CookSkeleton(input, output);
            } else if (type == "ANIMATION") {
                success = CookAnimation(input, output);
            }
            return success ? 0 : 1;
        } else {
            std::cerr << "Usage: AssetCooker COOK <TYPE> <INPUT> <OUTPUT>" << std::endl;
            return 1;
        }
    }

    // Interactive Mode
    std::cout << "[Cooker] Asset Cooker Service Started" << std::endl;
    std::cout << "[Cooker] Waiting for commands..." << std::endl;

    // Main loop: Read commands from stdin
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        ProcessCommand(line);
    }

    return 0;
}
