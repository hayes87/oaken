cmake_minimum_required(VERSION 3.20)

project(AssetCooker)

add_executable(AssetCooker
    Source/main.cpp
)

find_package(fastgltf CONFIG REQUIRED)
find_package(Stb REQUIRED)

target_link_libraries(AssetCooker PRIVATE 
    fastgltf::fastgltf 
)

target_include_directories(AssetCooker PRIVATE ${Stb_INCLUDE_DIR})

# Ensure it outputs to the same bin directory as the Engine/Game
set_target_properties(AssetCooker PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
)
