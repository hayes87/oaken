add_library(Sandbox SHARED
    Source/main.cpp
    Source/GamePlaySystem.cpp
    Source/GamePlaySystem.h
    Source/GameComponents.h
)

target_link_libraries(Sandbox PRIVATE OakenEngine)

# Rename output to Game.dll
set_target_properties(Sandbox PROPERTIES OUTPUT_NAME "Game")

# Ensure Runner is built before we try to copy it
# add_dependencies(Sandbox Runner)

# Copy Runner.exe to the Game output directory and rename it to Sandbox.exe
# We use a custom command that depends on the Runner executable file, not the target
# This avoids the build-time lock if Runner is running
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Runner>
    $<TARGET_FILE_DIR:Sandbox>/Sandbox.exe
    COMMENT "Copying Runner to Game Directory as Sandbox.exe"
)

# Copy Runtime DLLs (SDL3, flecs, fastgltf, etc.) to Game Directory
if(WIN32)
    add_custom_command(TARGET Sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:Sandbox>
        $<TARGET_FILE_DIR:Sandbox>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying Runtime DLLs to Game Directory"
    )
endif()

# Copy Shaders to Game Directory
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Dev/Engine/Source/Shaders
    $<TARGET_FILE_DIR:Sandbox>/Source/Shaders
    COMMENT "Copying Shaders to Game Directory"
)
