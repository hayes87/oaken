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

# Fix for MSVC Runtime Library Mismatch (LNK2038) and PDB access (C1041)
if(MSVC)
    add_compile_options(/FS)
    set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Configure ozz-animation
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
  ozz-animation
  GIT_REPOSITORY https://github.com/guillaumeblanc/ozz-animation.git
  GIT_TAG        master
)
set(ozz_build_msvc_rt_dll ON CACHE BOOL "Build with dynamic runtime" FORCE)
FetchContent_MakeAvailable(ozz-animation)

add_subdirectory(Engine)
add_subdirectory(AssetCooker)
add_subdirectory(Launcher)
add_subdirectory(Runner)
add_subdirectory(../Game Game)
