cmake_minimum_required(VERSION 3.20)

project(Launcher)

add_executable(Launcher
    Source/main.cpp
)

find_package(SDL3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

target_link_libraries(Launcher PRIVATE
    SDL3::SDL3
    imgui::imgui
)

if(WIN32)
    target_link_libraries(Launcher PRIVATE opengl32)
endif()

# Output to a top-level Oaken/bin folder
set_target_properties(Launcher PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Oaken/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}/Oaken/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Oaken/bin"
)

# Copy Runtime DLLs (SDL3, etc.) to Launcher Directory
if(WIN32)
    add_custom_command(TARGET Launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:Launcher>
        $<TARGET_FILE_DIR:Launcher>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying Runtime DLLs to Launcher Directory"
    )
endif()
