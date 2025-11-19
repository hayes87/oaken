cmake_minimum_required(VERSION 3.25)
project(OakenWorkspace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(SDL3 CONFIG REQUIRED)
find_package(flecs CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Jolt CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_path(SOL2_INCLUDE_DIRS "sol/sol.hpp")
find_package(spdlog CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(fastgltf CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

add_subdirectory(Engine)
add_subdirectory(Game)
