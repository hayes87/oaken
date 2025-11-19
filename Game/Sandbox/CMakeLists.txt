add_library(Sandbox SHARED
    Source/main.cpp
    Source/GamePlaySystem.cpp
    Source/GamePlaySystem.h
    Source/GameComponents.h
)

target_link_libraries(Sandbox PRIVATE OakenEngine)

# Copy Sandbox DLL to Runner directory for easy testing
# Note: We now have a separate Runner build dir, but let's keep a copy there if possible, 
# or just rely on the user passing the path.
# For convenience, let's copy it to the Runner's debug folder if it exists.
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Sandbox>
    ${CMAKE_BINARY_DIR}/Runner/Debug/Sandbox.dll
    COMMENT "Copying Sandbox DLL to Runner Directory"
)
