#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace fs = std::filesystem;

struct OakTexHeader {
    char signature[4] = {'O', 'A', 'K', 'T'};
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t channels = 0;
    uint32_t format = 0; // 0 = RGBA8
};

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
            fs::remove(output);
        }
        fs::rename(tempOutput, output);
        
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
    // TODO: Implement mesh cooking (e.g. gltf -> binary)
    std::cout << "[Cooker] Processing Mesh: " << input << " -> " << output << std::endl;
    return true;
}

void ProcessCommand(const std::string& commandLine) {
    std::stringstream ss(commandLine);
    std::string command;
    ss >> command;

    if (command == "COOK") {
        std::string type, inputPath, outputPath;
        ss >> type >> inputPath >> outputPath;

        bool success = false;
        if (type == "TEXTURE") {
            success = CookTexture(inputPath, outputPath);
        } else if (type == "MESH") {
            success = CookMesh(inputPath, outputPath);
        } else {
            std::cerr << "[Cooker] Unknown asset type: " << type << std::endl;
        }

        if (success) {
            std::cout << "SUCCESS " << outputPath << std::endl;
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
