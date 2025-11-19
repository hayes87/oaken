add_library(Sandbox SHARED
    Source/main.cpp
    Source/GamePlaySystem.cpp
    Source/GamePlaySystem.h
    Source/GameComponents.h
)

target_link_libraries(Sandbox PRIVATE OakenEngine)

# Rename output to Game.dll
set_target_properties(Sandbox PROPERTIES OUTPUT_NAME "Game")

# Copy Game DLL to Runner directory for easy testing
# Note: We now have a separate Runner build dir, but let's keep a copy there if possible, 
# or just rely on the user passing the path.
# For convenience, let's copy it to the Runner's debug folder if it exists.
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:Sandbox>
    ${CMAKE_BINARY_DIR}/Runner/Debug/Game.dll
    COMMENT "Copying Game DLL to Runner Directory"
)
