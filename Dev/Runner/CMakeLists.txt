add_executable(Runner
    Source/main.cpp
)

target_link_libraries(Runner PRIVATE OakenEngine)

# Copy Shaders to Build Directory
add_custom_command(TARGET Runner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Dev/Engine/Source/Shaders
    $<TARGET_FILE_DIR:Runner>/Source/Shaders
    COMMENT "Copying Shaders to Output Directory"
)

# Copy Engine DLL
add_custom_command(TARGET Runner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:OakenEngine>
    $<TARGET_FILE_DIR:Runner>
    COMMENT "Copying Engine DLL to Output Directory"
)

# Copy Runtime DLLs (SDL3, flecs, etc.)
if(WIN32)
    add_custom_command(TARGET Runner POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:Runner>
        $<TARGET_FILE_DIR:Runner>
        COMMAND_EXPAND_LISTS
        COMMENT "Copying Runtime DLLs"
    )
endif()
