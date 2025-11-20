#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <cstdlib>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace fs = std::filesystem;


// --- Application State ---
struct AppState {
    std::deque<std::string> logs;
    std::mutex logMutex;
    std::atomic<bool> isBusy{false};
    std::string statusMessage = "Ready";
    
    // Project Management
    std::vector<std::string> projects;
    int currentProjectIndex = 0;

    // Paths
    std::string buildDir = "Build";
    std::string config = "Debug";
    fs::path binDir;
    fs::path cookerExe;
    fs::path gameExe;
    fs::path assetSourceDir;
    fs::path assetCookedDir;

    void AddLog(const std::string& msg) {
        std::lock_guard<std::mutex> lock(logMutex);
        logs.push_back(msg);
        if (logs.size() > 1000) logs.pop_front();
        std::cout << msg << std::endl;
    }
};

AppState g_App;

// --- Helper Functions ---

void RunCommandAsync(const std::string& cmd, const std::string& successMsg) {
    g_App.isBusy = true;
    g_App.statusMessage = "Running: " + cmd;
    g_App.AddLog("[CMD] " + cmd);

    std::thread([cmd, successMsg]() {
        int result = std::system(cmd.c_str());
        
        if (result == 0) {
            g_App.AddLog("[SUCCESS] " + successMsg);
            g_App.statusMessage = "Ready";
        } else {
            g_App.AddLog("[ERROR] Command failed with code: " + std::to_string(result));
            g_App.statusMessage = "Error";
        }
        g_App.isBusy = false;
    }).detach();
}

void ScanProjects() {
    g_App.projects.clear();
    if (fs::exists("Game")) {
        for (const auto& entry : fs::directory_iterator("Game")) {
            if (entry.is_directory()) {
                // Simple check: assume any folder in Game is a project
                g_App.projects.push_back(entry.path().filename().string());
            }
        }
    }
    if (g_App.projects.empty()) {
        g_App.projects.push_back("Sandbox");
    }
}

void InitPaths() {
    std::string projectName = "Sandbox";
    if (g_App.currentProjectIndex >= 0 && g_App.currentProjectIndex < g_App.projects.size()) {
        projectName = g_App.projects[g_App.currentProjectIndex];
    }

    // Assuming running from Workspace Root
    g_App.binDir = fs::path(g_App.buildDir) / "bin" / g_App.config;
    g_App.cookerExe = g_App.binDir / "AssetCooker.exe";
    
    // Executable name matches project name (e.g. Sandbox.exe)
    g_App.gameExe = fs::path(g_App.buildDir) / "Game" / projectName / g_App.config / (projectName + ".exe");
    
    g_App.assetSourceDir = fs::path("Game") / projectName / "Assets";
    g_App.assetCookedDir = fs::path(g_App.buildDir) / "Game" / projectName / "Cooked" / "Assets";

    g_App.AddLog("Paths Initialized for Project: " + projectName);
    g_App.AddLog("  Cooker: " + g_App.cookerExe.string());
    g_App.AddLog("  Game:   " + g_App.gameExe.string());
    g_App.AddLog("  Assets In:  " + g_App.assetSourceDir.string());
    g_App.AddLog("  Assets Out: " + g_App.assetCookedDir.string());
}

int main(int argc, char** argv) {
    // Detect workspace root and set CWD if running from a subdirectory (e.g. Oaken/bin/)
    if (!fs::exists("Game")) {
        if (fs::exists("../Game")) {
            std::cout << "Switching CWD to Workspace Root (Up 1): " << fs::absolute("..") << std::endl;
            fs::current_path("..");
        } else if (fs::exists("../../Game")) {
            std::cout << "Switching CWD to Workspace Root (Up 2): " << fs::absolute("../..") << std::endl;
            fs::current_path("../..");
        }
    }

    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        std::cerr << "Error: SDL_Init(): " << SDL_GetError() << std::endl;
        return -1;
    }

    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    SDL_Window* window = SDL_CreateWindow("Oaken Launcher", 800, 600, window_flags);
    if (window == nullptr) {
        std::cerr << "Error: SDL_CreateWindow(): " << SDL_GetError() << std::endl;
        return -1;
    }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ScanProjects();
    InitPaths();

    // Main loop
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // UI Definition
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::Begin("Launcher", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

            ImGui::Text("Oaken Engine Launcher");
            ImGui::Separator();

            // Project Switcher
            if (!g_App.projects.empty()) {
                if (ImGui::BeginCombo("Project", g_App.projects[g_App.currentProjectIndex].c_str())) {
                    for (int i = 0; i < g_App.projects.size(); i++) {
                        bool isSelected = (g_App.currentProjectIndex == i);
                        if (ImGui::Selectable(g_App.projects[i].c_str(), isSelected)) {
                            g_App.currentProjectIndex = i;
                            InitPaths(); // Refresh paths
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            
            ImGui::Text("Status: %s", g_App.statusMessage.c_str());
            ImGui::Separator();

            ImGui::BeginDisabled(g_App.isBusy);

            if (ImGui::Button("1. Cook Assets", ImVec2(150, 40))) {
                if (fs::exists(g_App.cookerExe)) {
                    try {
                        fs::create_directories(g_App.assetCookedDir);
                        
                        // Iterate over assets and cook them one by one
                        // TODO: This should be done in a separate thread properly, but RunCommandAsync spawns a thread.
                        // However, spawning 100 threads for 100 assets is bad.
                        // For now, let's just cook one known file or implement a batch script.
                        // Better: Create a batch command string or loop in the thread.
                        
                        g_App.isBusy = true;
                        g_App.statusMessage = "Cooking Assets...";
                        g_App.AddLog("[CMD] Cooking Assets...");

                        std::thread([]() {
                            int successCount = 0;
                            int failCount = 0;
                            
                            if (fs::exists(g_App.assetSourceDir)) {
                                for (const auto& entry : fs::recursive_directory_iterator(g_App.assetSourceDir)) {
                                    if (entry.is_regular_file()) {
                                        std::string ext = entry.path().extension().string();
                                        if (ext == ".png" || ext == ".jpg") {
                                            fs::path relativePath = fs::relative(entry.path(), g_App.assetSourceDir);
                                            fs::path outputPath = g_App.assetCookedDir / relativePath;
                                            outputPath.replace_extension(".oaktex");
                                            
                                            fs::create_directories(outputPath.parent_path());
                                            
                                            std::string cmd = g_App.cookerExe.string() + " COOK TEXTURE \"" + entry.path().string() + "\" \"" + outputPath.string() + "\"";
                                            
                                            // Capture output using _popen
                                            FILE* pipe = _popen(cmd.c_str(), "r");
                                            if (pipe) {
                                                char buffer[128];
                                                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                                                    std::string line = buffer;
                                                    // Remove trailing newline
                                                    if (!line.empty() && line.back() == '\n') line.pop_back();
                                                    g_App.AddLog(line);
                                                }
                                                int result = _pclose(pipe);
                                                if (result == 0) successCount++;
                                                else failCount++;
                                            } else {
                                                g_App.AddLog("[ERROR] Failed to run cooker command.");
                                                failCount++;
                                            }
                                        }
                                    }
                                }
                            }
                            
                            g_App.AddLog("[COOK] Finished. Success: " + std::to_string(successCount) + ", Failed: " + std::to_string(failCount));
                            g_App.statusMessage = "Ready";
                            g_App.isBusy = false;
                        }).detach();

                    } catch (const std::exception& e) {
                        g_App.AddLog(std::string("[ERROR] Failed to cook: ") + e.what());
                    }
                } else {
                    g_App.AddLog("[ERROR] Cooker executable not found!");
                }
            }
            
            ImGui::SameLine();
            
            if (ImGui::Button("2. Build Game", ImVec2(150, 40))) {
                std::string cmd = "cmake --build " + g_App.buildDir + " --target Sandbox --config " + g_App.config;
                RunCommandAsync(cmd, "Game Built Successfully");
            }

            ImGui::SameLine();

            if (ImGui::Button("3. Launch Game", ImVec2(150, 40))) {
                if (fs::exists(g_App.gameExe)) {
                    std::string cmd = "cd " + g_App.gameExe.parent_path().string() + " && " + g_App.gameExe.filename().string();
                    RunCommandAsync(cmd, "Game Session Ended");
                } else {
                    g_App.AddLog("[ERROR] Game executable not found!");
                }
            }

            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::Text("Logs:");
            
            ImGui::BeginChild("LogRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            {
                std::lock_guard<std::mutex> lock(g_App.logMutex);
                for (const auto& log : g_App.logs) {
                    ImGui::TextUnformatted(log.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        SDL_GL_MakeCurrent(window, gl_context);
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}