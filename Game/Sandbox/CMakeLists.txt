add_executable(Sandbox
    Source/main.cpp
    Source/GamePlaySystem.cpp
    Source/GamePlaySystem.h
    Source/GameComponents.h
)

target_link_libraries(Sandbox PRIVATE OakenEngine)

# Copy Shaders to Build Directory
# Note: We are copying from Engine/Source/Shaders to the executable's directory
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Engine/Source/Shaders
    $<TARGET_FILE_DIR:Sandbox>/Source/Shaders
    COMMENT "Copying Shaders to Output Directory"
)

# Copy Engine DLL to Sandbox output directory
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:OakenEngine>
    $<TARGET_FILE_DIR:Sandbox>
    COMMENT "Copying Engine DLL to Output Directory"
)
