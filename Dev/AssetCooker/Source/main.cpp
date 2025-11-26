#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <fstream>
#include <iomanip> // For std::quoted
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp> // Added for make_mat4

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
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/transform.h>

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
    uint32_t boneCount = 0;        // Number of USED bones (compact count)
    uint32_t jointRemapCount = 0;  // Same as boneCount - for joint_remaps array
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 weights;
    glm::vec4 joints;  // Joint indices stored as floats for GPU compatibility
};

void RecurseSkeleton(const aiNode* node, ozz::animation::offline::RawSkeleton::Joint& joint) {
    joint.name = node->mName.C_Str();
    
    // Convert Assimp matrix to Ozz Float4x4
    ozz::math::Float4x4 ozzMat;
    const aiMatrix4x4& m = node->mTransformation;
    ozzMat.cols[0] = ozz::math::simd_float4::Load(m.a1, m.b1, m.c1, m.d1);
    ozzMat.cols[1] = ozz::math::simd_float4::Load(m.a2, m.b2, m.c2, m.d2);
    ozzMat.cols[2] = ozz::math::simd_float4::Load(m.a3, m.b3, m.c3, m.d3);
    ozzMat.cols[3] = ozz::math::simd_float4::Load(m.a4, m.b4, m.c4, m.d4);

    ozz::math::Transform transform;
    if (ozz::math::ToAffine(ozzMat, &transform)) {
        joint.transform = transform;
    } else {
        std::cerr << "[Cooker] Warning: Failed to decompose transform for node '" << joint.name << "'" << std::endl;
        aiVector3D scaling, position;
        aiQuaternion rotation;
        node->mTransformation.Decompose(scaling, rotation, position);
        
        joint.transform.translation = ozz::math::Float3(position.x, position.y, position.z);
        joint.transform.rotation = ozz::math::Quaternion(rotation.x, rotation.y, rotation.z, rotation.w);
        joint.transform.scale = ozz::math::Float3(scaling.x, scaling.y, scaling.z);
    }
    
    // Fix near-zero scales
    if (std::abs(joint.transform.scale.x) < 1e-4f || std::abs(joint.transform.scale.y) < 1e-4f || std::abs(joint.transform.scale.z) < 1e-4f) {
        joint.transform.scale = ozz::math::Float3(1.0f, 1.0f, 1.0f);
    }

    joint.children.resize(node->mNumChildren);
    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        RecurseSkeleton(node->mChildren[i], joint.children[i]);
    }
}

void RecurseMesh(const aiNode* node, const glm::mat4& parentTransform, const aiScene* scene, 
                 std::vector<Vertex>& vertices, std::vector<uint32_t>& indices, 
                 uint32_t& indexOffset, const std::unordered_map<std::string, int>& joint_map, int& matchedBones,
                 std::vector<glm::mat4>& inverseBindMatrices,
                 const std::vector<ozz::math::Float4x4>& bindPose) {
    
    aiMatrix4x4 m = node->mTransformation;
    glm::mat4 localTransform;
    // Transpose for GLM (Column-Major)
    localTransform[0][0] = m.a1; localTransform[0][1] = m.b1; localTransform[0][2] = m.c1; localTransform[0][3] = m.d1;
    localTransform[1][0] = m.a2; localTransform[1][1] = m.b2; localTransform[1][2] = m.c2; localTransform[1][3] = m.d2;
    localTransform[2][0] = m.a3; localTransform[2][1] = m.b3; localTransform[2][2] = m.c3; localTransform[2][3] = m.d3;
    localTransform[3][0] = m.a4; localTransform[3][1] = m.b4; localTransform[3][2] = m.c4; localTransform[3][3] = m.d4;

    glm::mat4 globalTransform = parentTransform * localTransform;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalTransform)));

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        std::cout << "[Cooker] Processing Mesh Node '" << node->mName.C_Str() << "' referencing Mesh " << node->mMeshes[i] << std::endl;
        
        uint32_t currentMeshVertexOffset = vertices.size();
        bool isSkinned = mesh->HasBones();
        
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            Vertex vertex;
            glm::vec4 pos = glm::vec4(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f);
            
            // Only bake transform if NOT skinned. Skinned meshes use IBMs and Bone Transforms.
            if (!isSkinned) {
                pos = globalTransform * pos;
            }
            
            vertex.position = glm::vec3(pos);
            
            if (mesh->HasNormals()) {
                glm::vec3 norm = glm::vec3(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                if (!isSkinned) {
                    norm = normalMatrix * norm;
                }
                vertex.normal = glm::normalize(norm);
            } else {
                vertex.normal = { 0.0f, 0.0f, 0.0f };
            }

            if (mesh->mTextureCoords[0]) {
                vertex.uv = { mesh->mTextureCoords[0][v].x, 1.0f - mesh->mTextureCoords[0][v].y };
            } else {
                vertex.uv = { 0.0f, 0.0f };
            }
            
            vertex.weights = {0.0f, 0.0f, 0.0f, 0.0f};
            vertex.joints = {0.0f, 0.0f, 0.0f, 0.0f};

            vertices.push_back(vertex);
        }

        if (mesh->HasBones()) {
            std::vector<int> boneCounts(mesh->mNumVertices, 0);
            std::set<int> usedJointIndices; // Track which joints are actually used
            
            for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                aiBone* bone = mesh->mBones[b];
                std::string boneName = bone->mName.C_Str();
                
                if (joint_map.find(boneName) == joint_map.end()) {
                    std::cout << "[Cooker] WARNING: Bone '" << boneName << "' not found in skeleton joint_map!" << std::endl;
                    continue;
                }
                int jointIndex = joint_map.at(boneName);
                matchedBones++;
                usedJointIndices.insert(jointIndex);
                
                // Use Assimp's offset matrix directly - this is the inverse bind matrix
                // It transforms from mesh space to bone space at bind pose
                const aiMatrix4x4& m = bone->mOffsetMatrix;
                glm::mat4 ibm;
                // Transpose for GLM (Column-Major)
                ibm[0][0] = m.a1; ibm[0][1] = m.b1; ibm[0][2] = m.c1; ibm[0][3] = m.d1;
                ibm[1][0] = m.a2; ibm[1][1] = m.b2; ibm[1][2] = m.c2; ibm[1][3] = m.d2;
                ibm[2][0] = m.a3; ibm[2][1] = m.b3; ibm[2][2] = m.c3; ibm[2][3] = m.d3;
                ibm[3][0] = m.a4; ibm[3][1] = m.b4; ibm[3][2] = m.c4; ibm[3][3] = m.d4;
                
                if (jointIndex < inverseBindMatrices.size()) {
                    inverseBindMatrices[jointIndex] = ibm;
                }

                for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                    aiVertexWeight weight = bone->mWeights[w];
                    uint32_t vertexId = weight.mVertexId;
                    float weightVal = weight.mWeight;
                    
                    if (boneCounts[vertexId] < 4) {
                        vertices[currentMeshVertexOffset + vertexId].weights[boneCounts[vertexId]] = weightVal;
                        vertices[currentMeshVertexOffset + vertexId].joints[boneCounts[vertexId]] = static_cast<float>(jointIndex);
                        boneCounts[vertexId]++;
                    }
                }
            }
            
            // Track first used joint for fallback
            int firstUsedJoint = usedJointIndices.empty() ? 0 : *usedJointIndices.begin();
            
            // Normalize weights
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                glm::vec4& w = vertices[currentMeshVertexOffset + v].weights;
                float total = w.x + w.y + w.z + w.w;
                if (total > 0.0f) {
                    w /= total;
                } else {
                    // Unweighted vertex! Assign to the first USED joint (one that has a valid IBM)
                    w = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
                    vertices[currentMeshVertexOffset + v].joints = glm::vec4(static_cast<float>(firstUsedJoint), 0.0f, 0.0f, 0.0f);
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

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        RecurseMesh(node->mChildren[i], globalTransform, scene, vertices, indices, indexOffset, joint_map, matchedBones, inverseBindMatrices, bindPose);
    }
}

bool CookSkeleton(const fs::path& input, const fs::path& output) {
    std::cout << "[Cooker] Processing Skeleton: " << input << " -> " << output << std::endl;

    Assimp::Importer importer;
    
    // aiProcess_OptimizeGraph collapses Assimp's intermediate $AssimpFbx$ nodes
    const aiScene* scene = importer.ReadFile(input.string(), 
        aiProcess_PopulateArmatureData | aiProcess_OptimizeGraph);

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
    
    // aiProcess_OptimizeGraph collapses Assimp's intermediate $AssimpFbx$ nodes
    const aiScene* scene = importer.ReadFile(input.string(), 
        aiProcess_PopulateArmatureData | aiProcess_OptimizeGraph);

    if (!scene || !scene->mRootNode) {
        std::cerr << "[Cooker] Assimp Error: " << importer.GetErrorString() << std::endl;
        return false;
    }

    if (!scene->HasAnimations()) {
        return true; // Not an error, just nothing to do
    }

    // For now, just take the first animation
    const aiAnimation* anim = scene->mAnimations[0];
    
    ozz::animation::offline::RawAnimation raw_animation;
    raw_animation.duration = (float)(anim->mDuration / anim->mTicksPerSecond);
    
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
    
    int channelsMatched = 0;
    for (unsigned int i = 0; i < anim->mNumChannels; ++i) {
        const aiNodeAnim* channel = anim->mChannels[i];
        std::string name = channel->mNodeName.C_Str();
        
        if (joint_map.find(name) == joint_map.end()) continue;
        channelsMatched++;
        
        int joint_index = joint_map[name];
        ozz::animation::offline::RawAnimation::JointTrack& track = raw_animation.tracks[joint_index];
        
        // Position keys - time in seconds (0 to duration)
        for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k) {
            const aiVectorKey& key = channel->mPositionKeys[k];
            ozz::animation::offline::RawAnimation::TranslationKey out_key;
            out_key.time = (float)(key.mTime / anim->mTicksPerSecond);
            out_key.value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
            track.translations.push_back(out_key);
        }
        
        // Rotation keys
        for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k) {
            const aiQuatKey& key = channel->mRotationKeys[k];
            ozz::animation::offline::RawAnimation::RotationKey out_key;
            out_key.time = (float)(key.mTime / anim->mTicksPerSecond);
            out_key.value = ozz::math::Quaternion(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
            track.rotations.push_back(out_key);
        }
        
        // Scale keys
        for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k) {
            const aiVectorKey& key = channel->mScalingKeys[k];
            ozz::animation::offline::RawAnimation::ScaleKey out_key;
            out_key.time = (float)(key.mTime / anim->mTicksPerSecond);
            out_key.value = ozz::math::Float3(key.mValue.x, key.mValue.y, key.mValue.z);
            track.scales.push_back(out_key);
        }
    }
    
    // For joints that have no animation channels, use rest pose keyframes at start and end
    int tracksInitialized = 0;
    
    // First, extract rest poses from skeleton's SoA format
    std::vector<ozz::math::Transform> restPoses(skeleton->num_joints());
    for (int j = 0; j < skeleton->num_joints(); ++j) {
        int soaIndex = j / 4;
        int lane = j % 4;
        const ozz::math::SoaTransform& soa = skeleton->joint_rest_poses()[soaIndex];
        float* transX = (float*)&soa.translation.x;
        float* transY = (float*)&soa.translation.y;
        float* transZ = (float*)&soa.translation.z;
        restPoses[j].translation = ozz::math::Float3(transX[lane], transY[lane], transZ[lane]);
        
        float* rotX = (float*)&soa.rotation.x;
        float* rotY = (float*)&soa.rotation.y;
        float* rotZ = (float*)&soa.rotation.z;
        float* rotW = (float*)&soa.rotation.w;
        restPoses[j].rotation = ozz::math::Quaternion(rotX[lane], rotY[lane], rotZ[lane], rotW[lane]);
        
        float* scaleX = (float*)&soa.scale.x;
        float* scaleY = (float*)&soa.scale.y;
        float* scaleZ = (float*)&soa.scale.z;
        restPoses[j].scale = ozz::math::Float3(scaleX[lane], scaleY[lane], scaleZ[lane]);
    }
    
    for (int j = 0; j < skeleton->num_joints(); ++j) {
        ozz::animation::offline::RawAnimation::JointTrack& track = raw_animation.tracks[j];
        
        bool needsInit = track.translations.empty() && track.rotations.empty() && track.scales.empty();
        if (needsInit) {
            // Use REST POSE for joints without animation data
            const ozz::math::Transform& rest = restPoses[j];
            
            ozz::animation::offline::RawAnimation::TranslationKey t0, t1;
            t0.time = 0.0f;
            t0.value = rest.translation;
            t1.time = raw_animation.duration;
            t1.value = rest.translation;
            track.translations.push_back(t0);
            track.translations.push_back(t1);
            
            ozz::animation::offline::RawAnimation::RotationKey r0, r1;
            r0.time = 0.0f;
            r0.value = rest.rotation;
            r1.time = raw_animation.duration;
            r1.value = rest.rotation;
            track.rotations.push_back(r0);
            track.rotations.push_back(r1);
            
            ozz::animation::offline::RawAnimation::ScaleKey s0, s1;
            s0.time = 0.0f;
            s0.value = rest.scale;
            s1.time = raw_animation.duration;
            s1.value = rest.scale;
            track.scales.push_back(s0);
            track.scales.push_back(s1);
            
            tracksInitialized++;
        }
    }
    
    // Validate before building
    if (!raw_animation.Validate()) {
        std::cerr << "[Cooker] Raw animation validation FAILED!" << std::endl;
        return false;
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
    std::cout << "[Cooker] Processing Mesh (COMPACT JOINTS): " << input << " -> " << output << std::endl;

    Assimp::Importer importer;
    
    // aiProcess_OptimizeGraph collapses Assimp's intermediate $AssimpFbx$ nodes
    const aiScene* scene = importer.ReadFile(input.string(), 
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_LimitBoneWeights | 
        aiProcess_PopulateArmatureData | aiProcess_OptimizeGraph);

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

    // ============================================
    // PASS 1: Collect all USED skeleton joint indices and their IBMs
    // ============================================
    std::set<int> usedSkeletonIndices;
    std::map<int, glm::mat4> skelIndexToIBM;
    
    std::function<void(const aiNode*)> collectUsedJoints = [&](const aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            if (!mesh->HasBones()) continue;
            
            for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                aiBone* bone = mesh->mBones[b];
                std::string boneName = bone->mName.C_Str();
                
                auto it = joint_map.find(boneName);
                if (it == joint_map.end()) continue;
                
                int skelIndex = it->second;
                usedSkeletonIndices.insert(skelIndex);
                
                const aiMatrix4x4& m = bone->mOffsetMatrix;
                glm::mat4 ibm;
                ibm[0][0] = m.a1; ibm[0][1] = m.b1; ibm[0][2] = m.c1; ibm[0][3] = m.d1;
                ibm[1][0] = m.a2; ibm[1][1] = m.b2; ibm[1][2] = m.c2; ibm[1][3] = m.d2;
                ibm[2][0] = m.a3; ibm[2][1] = m.b3; ibm[2][2] = m.c3; ibm[2][3] = m.d3;
                ibm[3][0] = m.a4; ibm[3][1] = m.b4; ibm[3][2] = m.c4; ibm[3][3] = m.d4;
                skelIndexToIBM[skelIndex] = ibm;
            }
        }
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            collectUsedJoints(node->mChildren[i]);
        }
    };
    collectUsedJoints(scene->mRootNode);
    
    // ============================================
    // Build COMPACT mapping: joint_remaps[compact] = skeleton_index
    // ============================================
    std::vector<uint16_t> joint_remaps;
    std::map<int, int> skelToCompact;
    
    int compactIdx = 0;
    for (int skelIdx : usedSkeletonIndices) {
        joint_remaps.push_back(static_cast<uint16_t>(skelIdx));
        skelToCompact[skelIdx] = compactIdx++;
    }
    
    // Build COMPACT inverse bind matrices
    std::vector<glm::mat4> compactIBMs;
    for (int skelIdx : usedSkeletonIndices) {
        compactIBMs.push_back(skelIndexToIBM[skelIdx]);
    }
    
    // ============================================
    // PASS 2: Process vertices with COMPACT indices
    // ============================================
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    uint32_t indexOffset = 0;
    
    std::function<void(const aiNode*, const glm::mat4&)> processMeshes = [&](const aiNode* node, const glm::mat4& parentTransform) {
        aiMatrix4x4 m = node->mTransformation;
        glm::mat4 localTransform;
        localTransform[0][0] = m.a1; localTransform[0][1] = m.b1; localTransform[0][2] = m.c1; localTransform[0][3] = m.d1;
        localTransform[1][0] = m.a2; localTransform[1][1] = m.b2; localTransform[1][2] = m.c2; localTransform[1][3] = m.d2;
        localTransform[2][0] = m.a3; localTransform[2][1] = m.b3; localTransform[2][2] = m.c3; localTransform[2][3] = m.d3;
        localTransform[3][0] = m.a4; localTransform[3][1] = m.b4; localTransform[3][2] = m.c4; localTransform[3][3] = m.d4;
        glm::mat4 globalTransform = parentTransform * localTransform;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(globalTransform)));
        
        for (unsigned int mi = 0; mi < node->mNumMeshes; mi++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[mi]];
            uint32_t meshVertexOffset = static_cast<uint32_t>(vertices.size());
            bool isSkinned = mesh->HasBones();
            
            // Add vertices
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                Vertex vertex;
                glm::vec4 pos(mesh->mVertices[v].x, mesh->mVertices[v].y, mesh->mVertices[v].z, 1.0f);
                
                // Only bake transform for NON-skinned meshes
                if (!isSkinned) {
                    pos = globalTransform * pos;
                }
                vertex.position = glm::vec3(pos);
                
                if (mesh->HasNormals()) {
                    glm::vec3 norm(mesh->mNormals[v].x, mesh->mNormals[v].y, mesh->mNormals[v].z);
                    if (!isSkinned) norm = normalMatrix * norm;
                    vertex.normal = glm::normalize(norm);
                } else {
                    vertex.normal = glm::vec3(0, 0, 0);
                }
                
                if (mesh->mTextureCoords[0]) {
                    vertex.uv = glm::vec2(mesh->mTextureCoords[0][v].x, 1.0f - mesh->mTextureCoords[0][v].y);
                } else {
                    vertex.uv = glm::vec2(0, 0);
                }
                
                vertex.weights = glm::vec4(0);
                vertex.joints = glm::vec4(0);  // Will use COMPACT indices
                
                vertices.push_back(vertex);
            }
            
            // Process bones -> assign COMPACT joint indices to vertices
            if (isSkinned) {
                std::vector<int> boneCounts(mesh->mNumVertices, 0);
                
                for (unsigned int b = 0; b < mesh->mNumBones; b++) {
                    aiBone* bone = mesh->mBones[b];
                    std::string boneName = bone->mName.C_Str();
                    
                    auto it = joint_map.find(boneName);
                    if (it == joint_map.end()) continue;
                    
                    int skelIndex = it->second;
                    auto compactIt = skelToCompact.find(skelIndex);
                    if (compactIt == skelToCompact.end()) continue;
                    
                    int compactIndex = compactIt->second;
                    
                    for (unsigned int w = 0; w < bone->mNumWeights; w++) {
                        uint32_t vertexId = bone->mWeights[w].mVertexId;
                        float weight = bone->mWeights[w].mWeight;
                        
                        int slot = boneCounts[vertexId];
                        if (slot < 4) {
                            vertices[meshVertexOffset + vertexId].weights[slot] = weight;
                            vertices[meshVertexOffset + vertexId].joints[slot] = static_cast<float>(compactIndex);
                            boneCounts[vertexId]++;
                        }
                    }
                }
                
                // Normalize weights, handle unweighted vertices
                for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                    glm::vec4& wts = vertices[meshVertexOffset + v].weights;
                    float total = wts.x + wts.y + wts.z + wts.w;
                    if (total > 0.0f) {
                        wts /= total;
                    } else {
                        // Unweighted vertex -> assign to first joint with weight 1
                        wts = glm::vec4(1.0f, 0, 0, 0);
                        vertices[meshVertexOffset + v].joints = glm::vec4(0, 0, 0, 0);
                    }
                }
            }
            
            // Add indices
            for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
                aiFace& face = mesh->mFaces[f];
                for (unsigned int j = 0; j < face.mNumIndices; j++) {
                    indices.push_back(face.mIndices[j] + indexOffset);
                }
            }
            indexOffset += mesh->mNumVertices;
        }
        
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processMeshes(node->mChildren[i], globalTransform);
        }
    };
    processMeshes(scene->mRootNode, glm::mat4(1.0f));

    // ============================================
    // Write output file
    // Format: Header | Vertices | Indices | IBMs | joint_remaps
    // ============================================
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
        header.boneCount = static_cast<uint32_t>(compactIBMs.size());
        header.jointRemapCount = static_cast<uint32_t>(joint_remaps.size());

        outFile.write(reinterpret_cast<const char*>(&header), sizeof(OakMeshHeader));
        outFile.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));
        outFile.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));
        
        if (header.boneCount > 0) {
            outFile.write(reinterpret_cast<const char*>(compactIBMs.data()), compactIBMs.size() * sizeof(glm::mat4));
        }
        
        if (header.jointRemapCount > 0) {
            outFile.write(reinterpret_cast<const char*>(joint_remaps.data()), joint_remaps.size() * sizeof(uint16_t));
        }

        outFile.close();

        if (fs::exists(output)) fs::remove(output);
        fs::rename(tempOutput, output);
        
        std::cout << "[Cooker] Mesh cooked successfully with COMPACT joints!" << std::endl;
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
        }
        std::cerr << "Usage: AssetCooker COOK <TYPE> <INPUT> <OUTPUT>" << std::endl;
        return 1;
    }

    // Interactive Mode (Service)
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        ProcessCommand(line);
    }

    return 0;
}
