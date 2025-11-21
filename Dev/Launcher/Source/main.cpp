#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <cstdlib>
#include <map>

#ifdef _WIN32
#include <windows.h>
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace fs = std::filesystem;

// --- Cooker Service ---
class CookerService {
public:
    CookerService() {}
    ~CookerService() { Stop(); }

    void Start(const std::string& exePath) {
        if (m_Running) return;
        
#ifdef _WIN32
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        // Create pipes
        if (!CreatePipe(&m_ChildOutRead, &m_ChildOutWrite, &saAttr, 0)) return;
        if (!SetHandleInformation(m_ChildOutRead, HANDLE_FLAG_INHERIT, 0)) return;

        if (!CreatePipe(&m_ChildInRead, &m_ChildInWrite, &saAttr, 0)) return;
        if (!SetHandleInformation(m_ChildInWrite, HANDLE_FLAG_INHERIT, 0)) return;

        STARTUPINFOA siStartInfo;
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
        siStartInfo.cb = sizeof(STARTUPINFO);
        siStartInfo.hStdError = m_ChildOutWrite;
        siStartInfo.hStdOutput = m_ChildOutWrite;
        siStartInfo.hStdInput = m_ChildInRead;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));

        std::string cmdLine = "\"" + exePath + "\""; // Quote path just in case

        if (!CreateProcessA(NULL, 
            const_cast<char*>(cmdLine.c_str()), 
            NULL, NULL, TRUE, 0, NULL, NULL, 
            &siStartInfo, &piProcInfo)) {
            return;
        }

        m_ProcessInfo = piProcInfo;
        m_Running = true;

        // Close handles we don't need
        CloseHandle(m_ChildOutWrite);
        CloseHandle(m_ChildInRead);

        // Start reading thread
        m_ReadThread = std::thread(&CookerService::ReadLoop, this);
#endif
    }

    void Stop() {
        if (!m_Running) return;
        
        SendCommand("EXIT");
        
        if (m_ReadThread.joinable()) m_ReadThread.join();

#ifdef _WIN32
        CloseHandle(m_ChildInWrite);
        CloseHandle(m_ChildOutRead);
        CloseHandle(m_ProcessInfo.hProcess);
        CloseHandle(m_ProcessInfo.hThread);
#endif
        m_Running = false;
    }

    void SendCommand(const std::string& cmd) {
        if (!m_Running) return;
        std::string fullCmd = cmd + "\n";
#ifdef _WIN32
        DWORD written;
        WriteFile(m_ChildInWrite, fullCmd.c_str(), (DWORD)fullCmd.length(), &written, NULL);
#endif
    }

    bool IsRunning() const { return m_Running; }

    std::deque<std::string> GetLogs() {
        std::lock_guard<std::mutex> lock(m_LogMutex);
        auto logs = m_Logs;
        m_Logs.clear();
        return logs;
    }

private:
    void ReadLoop() {
#ifdef _WIN32
        DWORD read;
        CHAR chBuf[4096];
        BOOL bSuccess = FALSE;

        while (m_Running) {
            bSuccess = ReadFile(m_ChildOutRead, chBuf, 4096, &read, NULL);
            if (!bSuccess || read == 0) {
                // Pipe broken or closed
                break;
            }

            std::string chunk(chBuf, read);
            // Split by newline and add to logs
            std::lock_guard<std::mutex> lock(m_LogMutex);
            // Simple split (not robust for partial lines but okay for now)
            size_t pos = 0;
            while ((pos = chunk.find('\n')) != std::string::npos) {
                m_Logs.push_back(chunk.substr(0, pos));
                chunk.erase(0, pos + 1);
            }
            if (!chunk.empty()) m_Logs.push_back(chunk);
        }
        
        if (m_Running) {
            std::lock_guard<std::mutex> lock(m_LogMutex);
            m_Logs.push_back("[SERVICE] Cooker Service Process Terminated Unexpectedly");
        }
        m_Running = false;
#endif
    }

    bool m_Running = false;
    std::thread m_ReadThread;
    std::mutex m_LogMutex;
    std::deque<std::string> m_Logs;

#ifdef _WIN32
    HANDLE m_ChildInRead = NULL;
    HANDLE m_ChildInWrite = NULL;
    HANDLE m_ChildOutRead = NULL;
    HANDLE m_ChildOutWrite = NULL;
    PROCESS_INFORMATION m_ProcessInfo;
#endif
};

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

    // Services
    CookerService cookerService;
    std::thread fileWatcherThread;
    std::atomic<bool> watchingFiles{false};

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

void StartFileWatcher() {
    if (g_App.watchingFiles) return;
    g_App.watchingFiles = true;
    
    g_App.fileWatcherThread = std::thread([]() {
        std::map<std::string, fs::file_time_type> fileTimes;
        
        while (g_App.watchingFiles) {
            if (fs::exists(g_App.assetSourceDir)) {
                for (const auto& entry : fs::recursive_directory_iterator(g_App.assetSourceDir)) {
                    if (entry.is_regular_file()) {
                        std::string path = entry.path().string();
                        std::string ext = entry.path().extension().string();
                        
                        // Map extensions to Cooker Commands
                        std::vector<std::string> cookCommands;
                        if (ext == ".png" || ext == ".jpg") {
                            cookCommands.push_back("COOK TEXTURE");
                        } else if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                            cookCommands.push_back("COOK MESH");
                            cookCommands.push_back("COOK SKELETON");
                            cookCommands.push_back("COOK ANIMATION");
                        } else if (ext == ".wav" || ext == ".mp3") {
                            cookCommands.push_back("COOK AUDIO");
                        } else if (ext == ".oakscene") {
                            cookCommands.push_back("COOK SCENE");
                        } else if (ext == ".vert" || ext == ".frag" || ext == ".comp") {
                            cookCommands.push_back("COOK SHADER");
                        }

                        if (!cookCommands.empty()) {
                            auto currentWriteTime = fs::last_write_time(entry.path());
                            
                            if (fileTimes.find(path) == fileTimes.end()) {
                                fileTimes[path] = currentWriteTime;
                            } else {
                                if (currentWriteTime != fileTimes[path]) {
                                    fileTimes[path] = currentWriteTime;
                                    
                                    // File Changed! Cook it.
                                    fs::path relativePath = fs::relative(entry.path(), g_App.assetSourceDir);
                                    fs::path outputPath = g_App.assetCookedDir / relativePath;
                                    fs::create_directories(outputPath.parent_path());
                                    
                                    for (const auto& cmdType : cookCommands) {
                                        fs::path finalOutput = outputPath;
                                        if (cmdType == "COOK TEXTURE") finalOutput.replace_extension(".oaktex");
                                        else if (cmdType == "COOK MESH") finalOutput.replace_extension(".oakmesh");
                                        else if (cmdType == "COOK SKELETON") finalOutput.replace_extension(".oakskel");
                                        else if (cmdType == "COOK ANIMATION") finalOutput.replace_extension(".oakanim");
                                        else if (cmdType == "COOK AUDIO") finalOutput.replace_extension(".oakaudio");
                                        else if (cmdType == "COOK SCENE") finalOutput.replace_extension(".oaklevel");
                                        else if (cmdType == "COOK SHADER") {
                                            // SPIR-V
                                            fs::path spvOut = outputPath;
                                            spvOut.replace_extension(ext + ".spv");
                                            std::string cmdSpv = cmdType + " \"" + entry.path().generic_string() + "\" \"" + spvOut.generic_string() + "\"";
                                            
                                            // DXIL
                                            fs::path dxilOut = outputPath;
                                            dxilOut.replace_extension(ext + ".dxil");
                                            std::string cmdDxil = cmdType + " \"" + entry.path().generic_string() + "\" \"" + dxilOut.generic_string() + "\"";

                                            if (g_App.cookerService.IsRunning()) {
                                                g_App.cookerService.SendCommand(cmdSpv);
                                                g_App.cookerService.SendCommand(cmdDxil);
                                                g_App.AddLog("[WATCHER] Requesting Cook: " + relativePath.string() + " (SHADER SPV+DXIL)");
                                            } else {
                                                g_App.AddLog("[WATCHER] Change detected but Service not running: " + relativePath.string());
                                            }
                                            continue; // Skip default command generation
                                        }
                                        
                                        std::string cmd = cmdType + " \"" + entry.path().generic_string() + "\" \"" + finalOutput.generic_string() + "\"";
                                        
                                        if (g_App.cookerService.IsRunning()) {
                                            g_App.cookerService.SendCommand(cmd);
                                            g_App.AddLog("[WATCHER] Requesting Cook: " + relativePath.string() + " (" + cmdType + ")");
                                        } else {
                                            g_App.AddLog("[WATCHER] Change detected but Service not running: " + relativePath.string());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });
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

    // Restart services if paths changed
    if (g_App.cookerService.IsRunning()) {
        g_App.cookerService.Stop();
    }
    
    if (fs::exists(g_App.cookerExe)) {
        g_App.cookerService.Start(g_App.cookerExe.string());
        g_App.AddLog("[SERVICE] Asset Cooker Started");
    } else {
        g_App.AddLog("[ERROR] Asset Cooker not found at " + g_App.cookerExe.string());
    }

    StartFileWatcher();
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
            if (g_App.cookerService.IsRunning()) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0, 1, 0, 1), "(Cooker Service Running)");
            } else {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "(Cooker Service Stopped)");
            }
            ImGui::Separator();

            ImGui::BeginDisabled(g_App.isBusy);

            if (ImGui::Button("1. Cook All Assets", ImVec2(150, 40))) {
                if (g_App.cookerService.IsRunning()) {
                    g_App.AddLog("[CMD] Requesting Full Cook...");
                    // Iterate and send commands
                    if (fs::exists(g_App.assetSourceDir)) {
                        for (const auto& entry : fs::recursive_directory_iterator(g_App.assetSourceDir)) {
                            if (entry.is_regular_file()) {
                                std::string ext = entry.path().extension().string();
                                fs::path relativePath = fs::relative(entry.path(), g_App.assetSourceDir);
                                fs::path outputPath = g_App.assetCookedDir / relativePath;
                                fs::create_directories(outputPath.parent_path());

                                if (ext == ".png" || ext == ".jpg") {
                                    outputPath.replace_extension(".oaktex");
                                    std::string cmd = "COOK TEXTURE \"" + entry.path().generic_string() + "\" \"" + outputPath.generic_string() + "\"";
                                    g_App.cookerService.SendCommand(cmd);
                                } else if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                                    // Mesh
                                    fs::path meshOut = outputPath;
                                    meshOut.replace_extension(".oakmesh");
                                    g_App.cookerService.SendCommand("COOK MESH \"" + entry.path().generic_string() + "\" \"" + meshOut.generic_string() + "\"");

                                    // Skeleton
                                    fs::path skelOut = outputPath;
                                    skelOut.replace_extension(".oakskel");
                                    g_App.cookerService.SendCommand("COOK SKELETON \"" + entry.path().generic_string() + "\" \"" + skelOut.generic_string() + "\"");

                                    // Animation
                                    fs::path animOut = outputPath;
                                    animOut.replace_extension(".oakanim");
                                    g_App.cookerService.SendCommand("COOK ANIMATION \"" + entry.path().generic_string() + "\" \"" + animOut.generic_string() + "\"");
                                } else if (ext == ".wav" || ext == ".mp3") {
                                    outputPath.replace_extension(".oakaudio");
                                    std::string cmd = "COOK AUDIO \"" + entry.path().generic_string() + "\" \"" + outputPath.generic_string() + "\"";
                                    g_App.cookerService.SendCommand(cmd);
                                } else if (ext == ".oakscene") {
                                    outputPath.replace_extension(".oaklevel");
                                    std::string cmd = "COOK SCENE \"" + entry.path().generic_string() + "\" \"" + outputPath.generic_string() + "\"";
                                    g_App.cookerService.SendCommand(cmd);
                                } else if (ext == ".vert" || ext == ".frag" || ext == ".comp") {
                                    // Cook SPIR-V
                                    fs::path spvOut = outputPath;
                                    spvOut.replace_extension(ext + ".spv");
                                    g_App.cookerService.SendCommand("COOK SHADER \"" + entry.path().generic_string() + "\" \"" + spvOut.generic_string() + "\"");
                                    
                                    // Cook DXIL
                                    fs::path dxilOut = outputPath;
                                    dxilOut.replace_extension(ext + ".dxil");
                                    g_App.cookerService.SendCommand("COOK SHADER \"" + entry.path().generic_string() + "\" \"" + dxilOut.generic_string() + "\"");
                                }
                            }
                        }
                    }
                } else {
                    g_App.AddLog("[ERROR] Cooker Service is not running.");
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
                // Poll logs from service
                auto serviceLogs = g_App.cookerService.GetLogs();
                for (const auto& log : serviceLogs) {
                    g_App.AddLog("[COOKER] " + log);
                }

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