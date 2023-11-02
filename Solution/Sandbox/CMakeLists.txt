# Sandbox Executable

file(GLOB_RECURSE SRC_FILES ./*.cpp)
add_executable (Game WIN32 ${SRC_FILES} )

target_include_directories(Game PUBLIC ${PROJECT_BINARY_DIR})

target_link_options(Game PUBLIC LINKER:-Map=Game.map)

target_link_libraries(Game PUBLIC
#user32.lib 
#gdi32.lib 
winmm.lib
Xinput.lib 
Engine
)
